/*
 *
 * Copyright (c) 2021, Brett Cameron
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __VMS
#include <inttypes.h>
#else
#include <stdint.h>
#endif
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#ifndef __VMS
#include <sys/select.h>
#endif
#ifndef _WIN32
#ifdef __VMS
#pragma names save
#pragma names uppercase
#endif
#include <pthread.h>
#ifdef __VMS
#pragma names restore
#endif
#endif

#ifdef _WIN32
#include <winsock2.h>
#endif				// _WIN32
#include "rmq.h"



#define OKAY(sts) (sts.reply_type == AMQP_RESPONSE_NORMAL ? 1 : 0)


#ifndef __VMS
#ifndef AMQP091
#define AMQP091
#endif
#endif				// __VMS


char *RabbitMQ_strerror(RMQ_conn_t * ch)
{
    RMQ_Assert(ch);
    return (ch->errstr);
}


static void
RabbitMQ_error(RMQ_conn_t * ch, amqp_rpc_reply_t x, char const *context)
{
    switch (x.reply_type) {
    case AMQP_RESPONSE_NONE:
	sprintf(ch->errstr, "%s: missing RPC reply type!\n", context);
	break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
	sprintf(ch->errstr, "%s: %s", context,
		amqp_error_string(x.library_error));
	break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
	switch (x.reply.id) {
	case AMQP_CONNECTION_CLOSE_METHOD:
	    {
		amqp_connection_close_t *m =
		    (amqp_connection_close_t *) x.reply.decoded;
		sprintf(ch->errstr,
			"%s: server connection error %d, message: %.*s",
			context, m->reply_code, (int) m->reply_text.len,
			(char *) m->reply_text.bytes);
		break;
	    }

	case AMQP_CHANNEL_CLOSE_METHOD:
	    {
		amqp_channel_close_t *m =
		    (amqp_channel_close_t *) x.reply.decoded;
		sprintf(ch->errstr,
			"%s: server channel error %d, message: %.*s",
			context, m->reply_code, (int) m->reply_text.len,
			(char *) m->reply_text.bytes);
		break;
	    }

	default:
	    sprintf(ch->errstr,
		    "%s: unknown server error, method id 0x%08X", context,
		    x.reply.id);
	    break;
	}

	break;

    default:
	sprintf(ch->errstr, "%s: unknown reply type (%d)", context,
		x.reply_type);
	break;
    }
}



static void RabbitMQ_syserror(RMQ_conn_t * ch, int sts, const char *str)
{
    if (sts < 0) {
	if (ch == NULL) {
	    fprintf(stderr, "## %s: %s\n", str, amqp_error_string(-sts));
	} else {
	    sprintf(ch->errstr, "%s: %s", str, amqp_error_string(-sts));
	}
    } else {
	if (ch == NULL) {
	    fprintf(stderr, "%s\n", str);
	} else {
	    strcpy(ch->errstr, str);
	}
    }
}



RMQ_conn_t *RabbitMQ_connect(char *url)
{
    RMQ_conn_t *ch = NULL;
    struct amqp_connection_info ci;
    amqp_rpc_reply_t rh;
    char *tmp = NULL;

    amqp_default_connection_info(&ci);

    /* Note that amqp_parse_url modifies the input string */
    if (url != NULL) {
	int rc;
	RMQ_AllocAssert((tmp = strdup(url)));
	if ((rc = amqp_parse_url(tmp, &ci)) != 0) {
	    RabbitMQ_syserror(NULL, rc, "");
	    goto hell;
	}
    }

    RMQ_AllocAssert((ch = (RMQ_conn_t *) malloc(sizeof(RMQ_conn_t))));

    /* Always set to 1 at the moment */
    ch->chan = 1;

    ch->fd = -1;
    ch->errstr[0] = '\0';

    /* Initialize RPC block (may never get used) */
    ch->rpc.name[0] = '\0';
    ch->rpc.repq.bytes = ch->rpc.name;
    ch->rpc.repq.len = sizeof(ch->rpc.name);
    ch->rpc.ph = NULL;
    ch->rpc.count = 0;

    RMQ_AllocAssert((ch->conn = amqp_new_connection()));

    ch->fd = amqp_open_socket(ci.host, ci.port);

    if (ch->fd < 0) {
	RabbitMQ_syserror(ch, ch->fd, "Unable to open socket");
	goto hell;
    }

    amqp_set_sockfd(ch->conn, ch->fd);

    rh = amqp_login(ch->conn, ci.vhost, RMQ_MAX_CHAN, RMQ_MAX_FRAME, 0,
		    AMQP_SASL_METHOD_PLAIN, ci.user, ci.password);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to login to broker");
	goto hell;
    }

    amqp_channel_open(ch->conn, ch->chan);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Error opening channel");
	goto hell;
    }

    if (tmp != NULL) {
	RMQ_Free(tmp);
    }

    return (ch);

  hell:
    if (tmp != NULL) {
	RMQ_Free(tmp);
    }

    if (ch != NULL) {
	if (ch->errstr[0]) {
	    fprintf(stderr, "## %s\n", RabbitMQ_strerror(ch));
	}

	if (ch->conn) {
	    if (ch->fd != -1) {
		amqp_channel_close(ch->conn, ch->chan, AMQP_REPLY_SUCCESS);
		amqp_connection_close(ch->conn, AMQP_REPLY_SUCCESS);
	    }

	    amqp_destroy_connection(ch->conn);
	}

	RMQ_Free(ch);
    }

    return (NULL);
}



void RabbitMQ_disconnect(RMQ_conn_t * ch)
{
    if (ch != NULL) {
	if (ch->conn) {
	    if (ch->fd != -1) {
		amqp_channel_close(ch->conn, ch->chan, AMQP_REPLY_SUCCESS);
		amqp_connection_close(ch->conn, AMQP_REPLY_SUCCESS);
	    }

	    amqp_destroy_connection(ch->conn);
	}

	RMQ_Free(ch);
    }
}



int
RabbitMQ_publish(RMQ_conn_t * ch, char *exchange, char *rkey,
		 int mandatory, int immediate,
		 amqp_basic_properties_t * prop, char *body, int len)
{
    amqp_bytes_t data;
    int rv;

    RMQ_Assert(ch);
    RMQ_Assert(exchange);
    RMQ_Assert(rkey);
    RMQ_Assert(body);

    ch->errstr[0] = '\0';

    data.bytes = body;

    if (len == -1) {
	data.len = strlen(body);
    } else {
	data.len = len;
    }

    rv = amqp_basic_publish(ch->conn, ch->chan,
			    amqp_cstring_bytes(exchange),
			    amqp_cstring_bytes(rkey), mandatory, immediate,
			    prop, data);

    if (rv < 0) {
	RabbitMQ_syserror(ch, ch->fd, "Unable to publish data");
    }

    return ((rv < 0 ? -1 : 0));
}


char *RabbitMQ_declare_queue(RMQ_conn_t * ch, char *name, int passive,
			     int durable, int exclusive, int auto_delete,
			     amqp_table_t * args)
{
    amqp_table_t table;
    amqp_bytes_t queue;
    amqp_queue_declare_ok_t *qd;
    amqp_rpc_reply_t rh;
    char *tmp;

    RMQ_Assert(ch);
    ch->errstr[0] = '\0';

    if (name == NULL || name[0] == '\0') {
	queue = amqp_empty_bytes;	/* Queue name will be generated */
    } else {
	queue.bytes = name;
	queue.len = strlen(name);
    }

    if (args == NULL) {
	table = amqp_empty_table;
    } else {
	table = *args;
    }

    qd = amqp_queue_declare(ch->conn, ch->chan, queue, passive, durable,
			    exclusive, auto_delete, table);

    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to create queue");
	return (NULL);
    }

    /* Return the queue name. Note that the caller needs to free this. */
    RMQ_AllocAssert((tmp =
		     (char *) malloc((qd->queue.len + 1) * sizeof(char))));
    memcpy(tmp, qd->queue.bytes, qd->queue.len);
    tmp[qd->queue.len] = '\0';

    return (tmp);
}



int
RabbitMQ_declare_exchange(RMQ_conn_t * ch, char *name, char *type,
			  int passive, int durable, amqp_table_t * args)
{
    amqp_table_t table;
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(name);
    RMQ_Assert(type);

    ch->errstr[0] = '\0';

    if (args == NULL) {
	table = amqp_empty_table;
    } else {
	table = *args;
    }

    amqp_exchange_declare(ch->conn, ch->chan, amqp_cstring_bytes(name),
			  amqp_cstring_bytes(type), passive, durable, 0, 0,
			  table);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to create exchange");
	return (-1);
    }

    return (0);
}


int
RabbitMQ_bind_queue(RMQ_conn_t * ch, char *name, char *exchange,
		    char *rkey, amqp_table_t * args)
{
    amqp_table_t table;
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(name);
    RMQ_Assert(exchange);
    RMQ_Assert(rkey);

    ch->errstr[0] = '\0';

    if (args == NULL) {
	table = amqp_empty_table;
    } else {
	table = *args;
    }

    amqp_queue_bind(ch->conn, ch->chan, amqp_cstring_bytes(name),
		    amqp_cstring_bytes(exchange), amqp_cstring_bytes(rkey),
		    table);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to bind queue");
	return (-1);
    }

    return (0);
}



int
RabbitMQ_unbind_queue(RMQ_conn_t * ch, char *name, char *exchange,
		      char *rkey, amqp_table_t * args)
{
    amqp_table_t table;
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(name);
    RMQ_Assert(exchange);
    RMQ_Assert(rkey);

    ch->errstr[0] = '\0';

    if (args == NULL) {
	table = amqp_empty_table;
    } else {
	table = *args;
    }

    amqp_queue_unbind(ch->conn, ch->chan, amqp_cstring_bytes(name),
		      amqp_cstring_bytes(exchange),
		      amqp_cstring_bytes(rkey), table);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to unbind queue");
	return (-1);
    }

    return (0);
}

/* ------------------------------------------------------------------------------------------------------- */

int
RabbitMQ_bind_exchange(RMQ_conn_t * ch, char *dest, char *from, char *rkey,
		       amqp_table_t * args)
{
#ifdef AMQP091
    amqp_table_t table;
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(dest);
    RMQ_Assert(from);
    RMQ_Assert(rkey);

    ch->errstr[0] = '\0';

    if (args == NULL) {
	table = amqp_empty_table;
    } else {
	table = *args;
    }

    amqp_exchange_bind(ch->conn, ch->chan, amqp_cstring_bytes(dest),
		       amqp_cstring_bytes(from), amqp_cstring_bytes(rkey),
		       table);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to bind exchange");
	return (-1);
    }

    return (0);
#else
    sprintf(ch->errstr, "Function unsupported by protocol version");
    return (-1);
#endif
}

/* ------------------------------------------------------------------------------------------------------- */

int
RabbitMQ_unbind_exchange(RMQ_conn_t * ch, char *dest, char *from,
			 char *rkey, amqp_table_t * args)
{
#ifdef AMQP091
    amqp_table_t table;
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(dest);
    RMQ_Assert(from);
    RMQ_Assert(rkey);

    ch->errstr[0] = '\0';

    if (args == NULL) {
	table = amqp_empty_table;
    } else {
	table = *args;
    }

    amqp_exchange_unbind(ch->conn, ch->chan, amqp_cstring_bytes(dest),
			 amqp_cstring_bytes(from),
			 amqp_cstring_bytes(rkey), table);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to un-bind exchange");
	return (-1);
    }

    return (0);
#else
    sprintf(ch->errstr, "Function unsupported by protocol version");
    return (-1);
#endif
}

/* ------------------------------------------------------------------------------------------------------- */

char *RabbitMQ_consume(RMQ_conn_t * ch, char *qnam, char *rkey, int no_lcl,
		       int no_ack, int exclsv, amqp_table_t * args)
{
    amqp_table_t table;
    amqp_rpc_reply_t rh;
    amqp_bytes_t ctag;
    char *tmp = NULL;
    amqp_basic_consume_ok_t *rv;

    RMQ_Assert(ch);
    RMQ_Assert(qnam);

    ch->errstr[0] = '\0';

    if (rkey == NULL || rkey[0] == '\0') {
	ctag = amqp_empty_bytes;
    } else {
	ctag.bytes = rkey;
	ctag.len = strlen(rkey);
    }


    if (args == NULL) {
	table = amqp_empty_table;
    } else {
	table = *args;
    }

#ifdef AMQP091
    rv = amqp_basic_consume(ch->conn, ch->chan, amqp_cstring_bytes(qnam),
			    ctag, no_lcl, no_ack, exclsv, table);
#else
    rv = amqp_basic_consume(ch->conn, ch->chan, amqp_cstring_bytes(qnam),
			    ctag, no_lcl, no_ack, exclsv);
#endif
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to consume messages");
	return (NULL);
    }

    /* Return the consunmer tag (or NULL) */
    RMQ_AllocAssert((tmp =
		     (char *) malloc((rv->consumer_tag.len + 1) *
				     sizeof(char))));
    memcpy(tmp, rv->consumer_tag.bytes, rv->consumer_tag.len);
    tmp[rv->consumer_tag.len] = '\0';
    return (tmp);
}

/* ------------------------------------------------------------------------------------------------------- */

void RabbitMQ_info_init(RMQ_info_t * data)
{
    RMQ_Free(data->rkey);
    RMQ_Free(data->repq);
    RMQ_Free(data->data.bytes);
    data->data.len = 0;
    data->dtag = 0;
    RMQ_Free(data->cid);
}

/* ------------------------------------------------------------------------------------------------------- */

static char *tostring(amqp_bytes_t dsc)
{
    char *tmp = NULL;

    RMQ_AllocAssert((tmp = (char *) malloc((dsc.len + 1) * sizeof(char))));
    memcpy(tmp, dsc.bytes, dsc.len);
    tmp[dsc.len] = '\0';
    return (tmp);
}


int
RabbitMQ_get(RMQ_conn_t * ch, const char *queue, RMQ_info_t * data,
	     int no_ack)
{
    char *tmp;
    int total_size;
    int total_read;
    int len;
    amqp_frame_t frame, *fp;
    int rv;
    amqp_rpc_reply_t rh;
    amqp_basic_properties_t *props;

    amqp_maybe_release_buffers(ch->conn);	/* Or risk running out of memory */

    fp = &frame;
    memset(data, '\0', sizeof(RMQ_info_t));

    rh = amqp_basic_get(ch->conn, ch->chan, amqp_cstring_bytes(queue),
			no_ack);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to get message");
	goto hell;
    }

    if (rh.reply.id == AMQP_BASIC_GET_EMPTY_METHOD) {
	return (0);
    }

    rv = amqp_simple_wait_frame(ch->conn, fp);

    if (rv < 0) {
	RabbitMQ_syserror(ch, rv, "Error receiving frame");
	goto hell;
    }

    if (fp->frame_type != AMQP_FRAME_HEADER) {
	sprintf(ch->errstr,
		"Expected header frame (%x) but found frame type %x",
		AMQP_FRAME_HEADER, fp->frame_type);
	goto hell;
    }

    props = fp->payload.properties.decoded;

    /* Reply queue */
    if (props->_flags & AMQP_BASIC_REPLY_TO_FLAG) {
	data->repq = tostring(props->reply_to);
    } else {
	data->repq = NULL;
    }

    /* Correlation ID */
    if (props->_flags & AMQP_BASIC_CORRELATION_ID_FLAG) {
	data->cid = tostring(props->correlation_id);
    } else {
	data->cid = NULL;
    }


    /* Get total message size */
    total_size = fp->payload.properties.body_size;
    RMQ_AllocAssert((data->data.bytes =
		     malloc(total_size * sizeof(char))));

    /* Now read the message */
    total_read = 0;

    while (total_read < total_size) {
	rv = amqp_simple_wait_frame(ch->conn, fp);

	if (rv < 0) {
	    RabbitMQ_syserror(ch, rv, "Error receiving frame");
	    goto hell;
	}

	if (fp->frame_type != AMQP_FRAME_BODY) {
	    sprintf(ch->errstr,
		    "Expected body frame (%x) but found frame type %x",
		    AMQP_FRAME_BODY, fp->frame_type);
	    goto hell;
	}

	len = fp->payload.body_fragment.len;
	tmp = fp->payload.body_fragment.bytes;

	if ((total_read + len) > total_size) {
	    strcpy(ch->errstr, "Received more data than expected");
	    goto hell;
	}

	memcpy(((char *) data->data.bytes + total_read), tmp, len);
	total_read += len;
    }

    data->data.len = total_size;
    return (0);

  hell:
    RabbitMQ_info_init(data);
    return (-1);
}

/* ------------------------------------------------------------------------------------------------------- */

int
RabbitMQ_dequeue(RMQ_conn_t * ch, RMQ_info_t * data, uint64_t * dtag,
		 int no_ack)
{
    char *tmp;
    int total_size;
    int total_read;
    amqp_basic_deliver_t *dp;
    int len;
    amqp_frame_t frame, *fp;
    int rv;
    amqp_basic_properties_t *ph;

    fp = &frame;

    /* Initialise this thing */
    memset(data, '\0', sizeof(RMQ_info_t));

  loop:
    amqp_maybe_release_buffers(ch->conn);

    rv = amqp_simple_wait_frame(ch->conn, fp);

    if (rv < 0) {
	RabbitMQ_syserror(ch, rv, "Error receiving frame");
	goto hell;
    }

    if (fp->frame_type == AMQP_FRAME_HEARTBEAT) {
	rv = amqp_send_frame(ch->conn, fp);

	if (rv < 0) {
	    RabbitMQ_syserror(ch, rv, "Error sending frame");
	    goto hell;
	} else {
	    goto loop;
	}
    } else if (fp->frame_type != AMQP_FRAME_METHOD) {
	goto loop;
    }

    if (fp->payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
	goto loop;
    }

    /* Delivery information */
    dp = (amqp_basic_deliver_t *) ((amqp_frame_t *) fp)->payload.method.
	decoded;

    /* Get routing key */
    data->rkey = tostring(dp->routing_key);

    rv = amqp_simple_wait_frame(ch->conn, fp);

    if (rv < 0) {
	RabbitMQ_syserror(ch, rv, "Error receiving frame");
	goto hell;
    }

    if (fp->frame_type != AMQP_FRAME_HEADER) {
	sprintf(ch->errstr,
		"Expected header frame (%x) but found frame type %x",
		AMQP_FRAME_HEADER, fp->frame_type);
	goto hell;
    }

    ph = fp->payload.properties.decoded;

    /* Reply queue */
    if (ph->_flags & AMQP_BASIC_REPLY_TO_FLAG) {
	data->repq = tostring(ph->reply_to);
    } else {
	data->repq = NULL;
    }

    /* Correlation ID */
    if (ph->_flags & AMQP_BASIC_CORRELATION_ID_FLAG) {
	data->cid = tostring(ph->correlation_id);
    } else {
	data->cid = NULL;
    }

    /* Get total message size */
    total_size = fp->payload.properties.body_size;
    RMQ_AllocAssert((data->data.bytes =
		     malloc(total_size * sizeof(char))));

    /* Now read the message */
    total_read = 0;

    while (total_read < total_size) {
	rv = amqp_simple_wait_frame(ch->conn, fp);

	if (rv < 0) {
	    RabbitMQ_syserror(ch, rv, "Error receiving frame");
	    goto hell;
	}

	if (fp->frame_type != AMQP_FRAME_BODY) {
	    sprintf(ch->errstr,
		    "Expected body frame (%x) but found frame type %x",
		    AMQP_FRAME_BODY, fp->frame_type);
	    goto hell;
	}

	len = fp->payload.body_fragment.len;
	tmp = fp->payload.body_fragment.bytes;

	if ((total_read + len) > total_size) {
	    strcpy(ch->errstr, "Received more data than expected");
	    goto hell;
	}

	memcpy(((char *) data->data.bytes + total_read), tmp, len);
	total_read += len;
    }


    /* If caller supplies somewhere to stick the frame tag, use it, otherwise do the acknowledgement here... */
    if (dtag != NULL) {
	data->dtag = dp->delivery_tag;
	*dtag = dp->delivery_tag;
    } else {
	data->dtag = 0;

	if (!no_ack) {
	    rv = amqp_basic_ack(ch->conn, ((amqp_frame_t *) fp)->channel,
				dp->delivery_tag, 0);

	    if (rv < 0) {
		RabbitMQ_syserror(ch, rv, "Failed to acknowledge message");
		goto hell;
	    }
	}
    }

    data->data.len = total_size;
    return (0);

  hell:
    RabbitMQ_info_init(data);
    return (-1);
}

/* ------------------------------------------------------------------------------------------------------- */

#ifdef _WIN32
#define I64_FMT "%I64X"
#else
#define I64_FMT "%llx"
#endif				// _WIN32

int
RabbitMQ_rpc_call(RMQ_conn_t * ch, char *exchange, char *rkey, char *body,
		  int len, RMQ_info_t * resp)
{
    int rv;
    amqp_queue_declare_ok_t *qd;
    amqp_bytes_t data;
    amqp_rpc_reply_t rh;
    char tmp[64];

    RMQ_Assert(ch);

    if (ch->rpc.ph == NULL) {
	RMQ_AllocAssert((ch->rpc.ph =
			 (amqp_basic_properties_t *) calloc(1,
							    sizeof
							    (amqp_basic_properties_t))));

	/* Declare queue */
	qd = amqp_queue_declare(ch->conn, ch->chan, amqp_empty_bytes, 0, 0,
				1, 1, amqp_empty_table);
	rh = amqp_get_rpc_reply(ch->conn);

	if (!OKAY(rh)) {
	    RabbitMQ_error(ch, rh, "Unable to create queue");
	    return (-1);
	}

	/* RPC reply queue */
	memcpy(ch->rpc.repq.bytes, qd->queue.bytes, qd->queue.len);
	ch->rpc.repq.len = qd->queue.len;

	ch->rpc.ph->_flags |= AMQP_BASIC_REPLY_TO_FLAG;
	ch->rpc.ph->reply_to = ch->rpc.repq;

	amqp_queue_bind(ch->conn, ch->chan, ch->rpc.repq,
			amqp_cstring_bytes(exchange), ch->rpc.repq,
			amqp_empty_table);
	rh = amqp_get_rpc_reply(ch->conn);

	if (!OKAY(rh)) {
	    RabbitMQ_error(ch, rh, "Unable to bind queue");
	    return (-1);
	}

	/* Note that "noack" is "true" (seems reasonable for RPC) */
#ifdef AMQP091
	amqp_basic_consume(ch->conn, ch->chan, ch->rpc.repq,
			   amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
#else
	amqp_basic_consume(ch->conn, ch->chan, ch->rpc.repq,
			   amqp_empty_bytes, 0, 1, 0);
#endif
	rh = amqp_get_rpc_reply(ch->conn);

	if (!OKAY(rh)) {
	    RabbitMQ_error(ch, rh, "Unable to consume from queue");
	    return (-1);
	}
    }

    ch->rpc.ph->_flags |= AMQP_BASIC_CORRELATION_ID_FLAG;
    ch->rpc.ph->correlation_id.bytes = tmp;
    ch->rpc.ph->correlation_id.len =
	sprintf(tmp, I64_FMT, ch->rpc.count++);

    data.bytes = body;

    if (len == -1) {
	data.len = strlen(body);
    } else {
	data.len = len;
    }

    rv = amqp_basic_publish(ch->conn, ch->chan,
			    amqp_cstring_bytes(exchange),
			    amqp_cstring_bytes(rkey), 0, 0, ch->rpc.ph,
			    data);

    if (rv < 0) {
	RabbitMQ_syserror(ch, ch->fd, "Unable to publish data");
	return (-1);
    }

    return (RabbitMQ_dequeue(ch, resp, NULL, 1));
}

/* ------------------------------------------------------------------------------------------------------- */

int
RabbitMQ_serve(RMQ_conn_t * ch, int (*func)(RMQ_info_t *, void *),
	       int flag, void *ud, int tout)
{
    RMQ_info_t data = { NULL, NULL, 0, NULL, { 0, NULL } };
    int rv;
    int fd;
    fd_set rfds;
    struct timeval tv;

    RMQ_Assert(ch);

    while (1) {
	if (tout >= 0) {
	    if (!amqp_frames_enqueued(ch->conn)
		&& !amqp_data_in_buffer(ch->conn)) {
		fd = amqp_get_sockfd(ch->conn);

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		tv.tv_sec = tout;
		tv.tv_usec = 0;

		rv = select(fd + 1, &rfds, NULL, NULL, &tv);

		switch (rv) {
		case -1:
		    sprintf(ch->errstr, "select(): %s", strerror(errno));
		    return (-1);
		    break;

		case 0:	/* Timed out */
		    return (0);
		    break;

		default:	/* There's something to read on fd */
		    break;
		}
	    }
	}

	if ((rv = RabbitMQ_dequeue(ch, &data, NULL, flag)) < 0) {
	    break;
	}

	rv = (*func) (&data, ud);
	RabbitMQ_info_init(&data);	/* Free up allocated memory */

	if (rv == -1) {
	    break;
	}
    }

    return (rv);
}

#ifndef _WIN32
typedef struct {
    RMQ_conn_t *ch;
    int (*func)(RMQ_info_t *, void *);
    int flag;
    void *ud;
    int tout;
} RMQ_serve_t;

static void _serve(void *args)
{
    RMQ_serve_t *ap = (RMQ_serve_t *) args;
    RabbitMQ_serve(ap->ch, ap->func, ap->flag, ap->ud, ap->tout);
}


pthread_t
RabbitMQ_serve_thread(RMQ_conn_t * ch, int (*func)(RMQ_info_t *, void *),
		      int flag, void *ud, int tout)
{
    pthread_t tid;
    RMQ_serve_t args;

    args.ch = ch;
    args.func = func;
    args.flag = flag;
    args.ud = ud;
    args.tout = tout;

    pthread_create(&tid, NULL, (void *(*)(void *)) _serve, &args);
#ifdef __VMS
    sleep(1);
#endif
    return (tid);
}
#endif



int RabbitMQ_purge_queue(RMQ_conn_t * ch, char *qnam)
{
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(qnam);

    ch->errstr[0] = '\0';

    amqp_queue_purge(ch->conn, ch->chan, amqp_cstring_bytes(qnam));
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to purge queue");
	return (-1);
    }

    return (0);
}



int
RabbitMQ_delete_queue(RMQ_conn_t * ch, char *qnam, int if_unused,
		      int if_nomsgs)
{
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(qnam);

    ch->errstr[0] = '\0';

    amqp_queue_delete(ch->conn, ch->chan, amqp_cstring_bytes(qnam),
		      if_unused, if_nomsgs);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to delete queue");
	return (-1);
    }

    return (0);
}



int
RabbitMQ_delete_exchange(RMQ_conn_t * ch, char *exchange, int if_unused)
{
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    RMQ_Assert(exchange);

    ch->errstr[0] = '\0';

    amqp_exchange_delete(ch->conn, ch->chan, amqp_cstring_bytes(exchange),
			 if_unused);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to delete exchange");
	return (-1);
    }

    return (0);
}



int RabbitMQ_tx_select(RMQ_conn_t * ch)
{
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    ch->errstr[0] = '\0';

    amqp_tx_select(ch->conn, ch->chan);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to start transaction");
	return (-1);
    }

    return (0);
}



int RabbitMQ_tx_commit(RMQ_conn_t * ch)
{
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    ch->errstr[0] = '\0';

    amqp_tx_commit(ch->conn, ch->chan);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to commit transaction");
	return (-1);
    }

    return (0);
}




int RabbitMQ_tx_rollback(RMQ_conn_t * ch)
{
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    ch->errstr[0] = '\0';

    amqp_tx_rollback(ch->conn, ch->chan);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to rollback");
	return (-1);
    }

    return (0);
}




int RabbitMQ_cancel(RMQ_conn_t * ch, char *consumer_tag)
{
    amqp_rpc_reply_t rh;
    amqp_bytes_t tmp;

    RMQ_Assert(ch);
    RMQ_Assert(consumer_tag);

    ch->errstr[0] = '\0';

    tmp.bytes = consumer_tag;
    tmp.len = strlen(consumer_tag);

    amqp_basic_cancel(ch->conn, ch->chan, tmp);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to cancel consume");
	return (-1);
    }

    return (0);
}

/* ------------------------------------------------------------------------------------------------------- */

int RabbitMQ_qos(RMQ_conn_t * ch, int size, int count, int flag)
{
    amqp_rpc_reply_t rh;

    RMQ_Assert(ch);
    ch->errstr[0] = '\0';

    /* global=true is not currently supported so we always pass in 0, and size must also be 0 */
    amqp_basic_qos(ch->conn, ch->chan, 0, count, 0);
    rh = amqp_get_rpc_reply(ch->conn);

    if (!OKAY(rh)) {
	RabbitMQ_error(ch, rh, "Unable to set QoS");
	return (-1);
    }

    return (0);
}

/* ------------------------------------------------------------------------------------------------------- */

void RabbitMQ_free_info(RMQ_info_t * data)
{
    if (data != NULL) {
	RMQ_Free(data->rkey);
	RMQ_Free(data->repq);
	RMQ_Free(data->data.bytes);
	RMQ_Free(data->cid);
	RMQ_Free(data);
    }
}




RMQ_info_t *RabbitMQ_alloc_info()
{
    RMQ_info_t *tmp;
    RMQ_AllocAssert((tmp = (RMQ_info_t *) calloc(1, sizeof(RMQ_info_t))));
    return (tmp);
}




void RabbitMQ_release(RMQ_conn_t * ch)
{
    RMQ_Assert(ch);
    if (amqp_release_buffers_ok(ch->conn)) {
	amqp_release_buffers(ch->conn);
    }
}



#define BPL 16

void RabbitMQ_dump(char *addr, int len)
{
    unsigned char *tmp = (unsigned char *) addr;
    int i;
    int j;

    for (i = 0; i < len; i += BPL) {
	fprintf(stderr, "%04x  ", i);

	for (j = i; j < len && (j - i) < BPL; j++) {
	    fprintf(stderr, "%02x ", tmp[j]);
	}

	for (; 0 != (j % BPL); j++) {
	    fprintf(stderr, "   ");
	}

	fprintf(stderr, "  |");

	for (j = i; j < len && (j - i) < BPL; j++) {
	    fprintf(stderr, "%c", (isprint(tmp[j])) ? tmp[j] : '.');
	}

	fprintf(stderr, "|\n");
    }
}
