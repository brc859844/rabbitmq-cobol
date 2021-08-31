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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#include <amqp.h>
#include <amqp_framing.h>
#include "utils.h"
#include "list.h"
#include "hash.h"


#define SVRINIT "AMQP_SVRINIT"
#define SVRDONE "AMQP_SVRDONE"


typedef struct {
    char *routing_key;
    size_t key_len;
    amqp_bytes_t idata;
    amqp_bytes_t odata;
} svcinfo_t;


typedef struct {
    amqp_connection_state_t conn;
    char *exchange;
    char *queue;
    adc_HT_t *ht;
    int (*init) (int, char **);
    int (*done) ();
} gbl_t;


typedef struct {
    char *routing_key;
    size_t key_len;
    char *name;
    void (*func) (void *, char *, size_t *, char **, size_t *);
} info_t;


typedef enum {			/* Really only using INFO and FATAL for the moment */
    INFO,
    WARN,
    ERROR,
    FATAL
} severity_t;


#define DEF_USER "guest"
#define DEF_PASSWORD "guest"
#define DEF_PORT 5672
#define DEF_VHOST "/"
#define DEF_EXCHANGE "amq.direct"

#ifndef HT_LEN
#define HT_LEN 257
#endif


static int debug = 0;
static int trace = 0;


#define OKAY(x) ((x).reply_type == AMQP_RESPONSE_NORMAL)




static void ulog(severity_t severity, char *fmt, ...)
{
    FILE *fp = stderr;
    va_list ap;
    char tmp[32];
    time_t t;

    /* Get current date and time in our preferred format */
    t = time(NULL);
    strftime(tmp, sizeof(tmp) - 1, "%d/%m/%y %H:%M:%S", localtime(&t));

    /* Write log entry */
    fprintf(fp, "[%s] ", tmp);

    if ((severity == FATAL) || (severity == ERROR)) {
	fprintf(fp, "[%s] FATAL: ", tmp);
    } else {
	fprintf(fp, "[%s] ", tmp);
    }

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);

    fprintf(fp, "\n");
    fflush(fp);
    fsync(fileno(fp));

    if ((severity == FATAL) || (severity == ERROR)) {
	exit(EXIT_FAILURE);
    }
}


static char *getmsg(amqp_rpc_reply_t rh, char const *str)
{
    static char errstr[256];

    errstr[0] = '\0';

    switch (rh.reply_type) {
    case AMQP_RESPONSE_NORMAL:
	break;

    case AMQP_RESPONSE_NONE:
	sprintf(errstr, "%s: missing RPC reply type (no response)", str);
	break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
	sprintf(errstr, "%s: %s", str,
		amqp_error_string(rh.library_error));
	break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
	switch (rh.reply.id) {
	case AMQP_CONNECTION_CLOSE_METHOD:
	    {
		amqp_connection_close_t *m =
		    (amqp_connection_close_t *) rh.reply.decoded;

		sprintf(errstr,
			"%s: server connection error %d; message: %.*s",
			str, m->reply_code, (int) m->reply_text.len,
			(char *) m->reply_text.bytes);
		break;
	    }

	case AMQP_CHANNEL_CLOSE_METHOD:
	    {
		amqp_channel_close_t *m =
		    (amqp_channel_close_t *) rh.reply.decoded;

		sprintf(errstr,
			"%s: server channel error %d; message: %.*s", str,
			m->reply_code, (int) m->reply_text.len,
			(char *) m->reply_text.bytes);
		break;
	    }

	default:
	    sprintf(errstr, "%s: unknown server error (method id 0x%08X)",
		    str, rh.reply.id);
	    break;
	}

	break;

    default:
	sprintf(errstr, "%s: unknown error condition (%d)", str,
		rh.reply_type);
	break;
    }

    return (errstr);
}


#define MULTIPLIER 31

static unsigned int _hash(const void *ent)
{
    unsigned int h = 0;
    int i;
    info_t *tmp = (info_t *) ent;

    for (i = 0; i < tmp->key_len; i++) {
	h = MULTIPLIER * h + tmp->routing_key[i];
    }

    return (h);
}


static int _match(const void *v1, const void *v2)
{
    info_t *t1 = (info_t *) v1;
    info_t *t2 = (info_t *) v2;

    if (t1->key_len != t2->key_len) {
	return (-1);
    }

    return (strncmp(t1->routing_key, t2->routing_key, t1->key_len));
}


static void _destroy(void *ent)
{
    /* Never required */
}


static info_t *_lookup(adc_HT_t * ht, char *routing_key, size_t key_len)
{
    info_t ent, *tmp;

    ent.routing_key = routing_key;
    ent.key_len = key_len;

    tmp = &ent;

    if (adc_HT_Lookup(ht, (void **) &tmp) == 0) {
	return (tmp);
    }

    return (NULL);
}


static void addkey(adc_HT_t * ht, const char *str)
{
    info_t *info = NULL;
    char *tmp;

    assert((info = (info_t *) malloc(sizeof(info_t))));

    /* Get the routing key and (optional) service name */
    if ((tmp = strchr(str, ':')) == NULL) {
	assert((info->routing_key = strdup(str)));
	assert((info->name = strdup(str)));

	info->key_len = strlen(info->routing_key);
    } else {
	info->key_len = (tmp - str);

	assert((info->routing_key =
		(char *) malloc((info->key_len + 1) * sizeof(char))));
	memcpy(info->routing_key, str, info->key_len);
	info->routing_key[info->key_len] = '\0';

	tmp++;

	assert((info->name = strdup(tmp)));
    }

    /* We will sort out the address of the callback function later */
    info->func = NULL;

    /* Add the entry into the hash table */
    if (adc_HT_Insert(ht, (const void *) info) != 0) {
	ulog(FATAL,
	     "Unable to add entry to hash table (possible duplicate)");
    }
}


static void bindkey(const void *ent, void *ud)
{
    info_t *tmp = (info_t *) ent;
    amqp_rpc_reply_t rh;
    gbl_t *gbl = (gbl_t *) ud;

    amqp_queue_bind(gbl->conn, 1, amqp_cstring_bytes(gbl->queue),
		    amqp_cstring_bytes(gbl->exchange),
		    amqp_cstring_bytes(tmp->routing_key),
		    amqp_empty_table);

    rh = amqp_get_rpc_reply(gbl->conn);

    if (!OKAY(rh)) {
	ulog(FATAL, getmsg(rh, "Unable to create binding"));
    }
}


static void addsym(const void *ent, void *ip)
{
    info_t *tmp = (info_t *) ent;

    if ((tmp->func = dlsym(ip, tmp->name)) == NULL) {
	ulog(FATAL, "dlsym(..., \"%s\"): %s", tmp->name, dlerror());
    }
}


static int load(adc_HT_t * ht, const char *file)
{
    FILE *fp = NULL;
    int l;
    int n;
    char line[256];
    char *t;

    ulog(INFO, "Reading service details from %s", file);

    if ((fp = fopen(file, "r")) == NULL) {
	ulog(FATAL, "fopen() %s", strerror(errno));
    }

    n = 0;

    while (fgets(line, sizeof(line) - 1, fp)) {
	l = strlen(line) - 1;
	line[l] = '\0';

	for (t = line + l - 1; t >= line && *t == ' '; t--) {
	    *t = '\0';
	}

	if (line[0] != '\0' && line[0] != '#' && line[0] != '!') {
	    addkey(ht, line);
	    n++;
	}
    }

    fclose(fp);
    return (n);
}


static void dequeue(amqp_connection_state_t conn, svcinfo_t * data,
		    amqp_bytes_t * rep, amqp_bytes_t * cid, uint64_t * tag)
{
    char *tmp;
    int total_size;
    int total_read;
    amqp_basic_deliver_t *dp;
    int len;
    amqp_frame_t frame, *fp;
    int rv;
    amqp_basic_properties_t *props;
    amqp_bytes_t dsc;

    fp = &frame;

    /* Initialise this thing */
    memset(data, '\0', sizeof(svcinfo_t));

  loop:
    amqp_maybe_release_buffers(conn);

    if ((rv = amqp_simple_wait_frame(conn, fp)) < 0) {
	ulog(FATAL, "Error receiving frame: %s", amqp_error_string(-rv));
    }

    if (fp->frame_type == AMQP_FRAME_HEARTBEAT) {
	rv = amqp_send_frame(conn, fp);

	if (rv < 0) {
	    ulog(FATAL, "Error sending frame: %s", amqp_error_string(-rv));
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
    dp = (amqp_basic_deliver_t *) ((amqp_frame_t *) fp)->payload.
	method.decoded;

    /* Get routing key */
    assert((data->routing_key =
	    (char *) malloc((dp->routing_key.len + 1) * sizeof(char))));
    data->key_len = dp->routing_key.len;
    memcpy(data->routing_key, dp->routing_key.bytes, dp->routing_key.len);

    if ((rv = amqp_simple_wait_frame(conn, fp)) < 0) {
	ulog(FATAL, "Error receiving frame: %s", amqp_error_string(-rv));
    }

    if (fp->frame_type != AMQP_FRAME_HEADER) {
	ulog(FATAL, "Expected header frame (%x) but found frame type %x",
	     AMQP_FRAME_HEADER, fp->frame_type);
    }

    props = fp->payload.properties.decoded;

    /* Reply queue */
    if (rep != NULL) {
	if (props->_flags & AMQP_BASIC_REPLY_TO_FLAG) {
	    dsc = props->reply_to;

	    if (dsc.len > rep->len) {
		ulog(FATAL, "Reply queue name too long (%d; %d)", dsc.len,
		     rep->len);
	    }

	    memcpy(rep->bytes, dsc.bytes, dsc.len);
	    rep->len = dsc.len;
	} else {
	    rep->bytes = NULL;
	    rep->len = 0;
	}
    }


    /* Correlation ID */
    if (cid != NULL) {
	if (props->_flags & AMQP_BASIC_CORRELATION_ID_FLAG) {
	    dsc = props->correlation_id;

	    if (dsc.len > cid->len) {
		ulog(FATAL, "Correlation ID too long (%d; %d)", dsc.len,
		     cid->len);
	    }

	    memcpy(cid->bytes, dsc.bytes, dsc.len);
	    cid->len = dsc.len;
	} else {
	    cid->bytes = NULL;
	    cid->len = 0;
	}
    }


    /* Get total message size */
    total_size = fp->payload.properties.body_size;
    assert((data->idata.bytes = malloc(total_size * sizeof(char))));


    /* Now read the message */

    total_read = 0;

    while (total_read < total_size) {
	if ((rv = amqp_simple_wait_frame(conn, fp)) < 0) {
	    ulog(FATAL, "Error receiving frame: %s",
		 amqp_error_string(-rv));
	}

	if (fp->frame_type != AMQP_FRAME_BODY) {
	    ulog(FATAL, "Expected body frame (%x) but found frame type %x",
		 AMQP_FRAME_BODY, fp->frame_type);
	}

	len = fp->payload.body_fragment.len;
	tmp = fp->payload.body_fragment.bytes;

	if ((total_read + len) > total_size) {
	    ulog(FATAL, "Received more data than expected");
	}

	memcpy((data->idata.bytes + total_read), tmp, len);
	total_read += len;
    }

    /* If caller supplies somewhere to stick the deliviery tag, use it,
       otherwise do the acknowledgement here... */

    if (tag != NULL) {
	*tag = dp->delivery_tag;
    } else {
	if ((rv =
	     amqp_basic_ack(conn, ((amqp_frame_t *) fp)->channel,
			    dp->delivery_tag, 0)) < 0) {
	    ulog(FATAL, "Failed to acknowledge message: %s",
		 amqp_error_string(-rv));
	}
    }

    data->idata.len = total_size;
}


static int serve(gbl_t * gbl)
{
    info_t *info;
    amqp_basic_properties_t props;
    uint64_t tag;
    char cid[64];		// Macro (TBD)
    char rep[64];		// Macro (TBD)
    amqp_bytes_t cid_dsc;
    amqp_bytes_t rep_dsc;
    svcinfo_t data;
    int rv;

    while (1) {
	memset(&props, '\0', sizeof(props));

	cid[0] = '\0';
	rep[0] = '\0';

	cid_dsc.bytes = cid;
	cid_dsc.len = sizeof(cid);
	rep_dsc.bytes = rep;
	rep_dsc.len = sizeof(rep);

	dequeue(gbl->conn, &data, &rep_dsc, &cid_dsc, &tag);

	if (debug) {
	    ulog(INFO, "Message received:\n"
		 "Length        : %ld bytes\n"
		 "Routing key   : %.*s     \n"
		 "Reply queue   : %.*s     \n"
		 "Correlation ID: %.*s     \n"
		 "Frame tag     : %lld     \n",
		 data.idata.len,
		 data.key_len,
		 data.routing_key,
		 rep_dsc.len, rep_dsc.bytes,
		 cid_dsc.len, cid_dsc.bytes, tag);
	}

	if (trace) {
	    amqp_dump(data.idata.bytes, data.idata.len);
	}

	if ((info =
	     _lookup(gbl->ht, data.routing_key, data.key_len)) != NULL) {
	    if (debug) {
		ulog(INFO, "Calling user routine \"%s\"", info->name);
	    }

	    info->func(NULL, data.idata.bytes, &data.idata.len,
		       (char **) &data.odata.bytes, &data.odata.len);
	}

	if ((rep_dsc.len != 0) && (rep[0] != '\0')) {
	    if (data.odata.len == 0) {
		ulog(FATAL,
		     "Reply queue specified but response buffer is empty");
	    }

	    if (debug) {
		ulog(INFO, "Sending response (%ld bytes)", data.odata.len);
	    }

	    if (trace) {
		amqp_dump(data.odata.bytes, data.odata.len);
	    }

	    if (cid[0] != '\0') {
		props._flags |= AMQP_BASIC_CORRELATION_ID_FLAG;
		props.correlation_id = cid_dsc;
	    } else {
		/* Properties already previously cleared */
	    }


	    /* Note that when working with the RabbitMQ RpcClient Java class we must return response data via the default exchange */
	    if ((rv =
		 amqp_basic_publish(gbl->conn, 1, amqp_empty_bytes,
				    rep_dsc, 0, 0, &props,
				    data.odata)) < 0) {
		ulog(FATAL, "Error publishing response: %s",
		     amqp_error_string(-rv));
	    }

	    if ((rv = amqp_basic_ack(gbl->conn, 1, tag, 0)) < 0) {
		ulog(FATAL, "Failed to acknowledge message: %s",
		     amqp_error_string(-rv));
	    }
	} else {
	    if ((rv = amqp_basic_ack(gbl->conn, 1, tag, 0)) < 0) {
		ulog(FATAL, "Failed to acknowledge message: %s",
		     amqp_error_string(-rv));
	    }

	    if (debug) {
		ulog(INFO, "No reply queue specified (okay)");
	    }
	}

	free(data.idata.bytes);
	free(data.routing_key);

	if (data.odata.bytes) {
	    free(data.odata.bytes);
	    data.odata.bytes = NULL;
	}
    }

    /* Never get here. We possibly need some way of being signalled to break out of the
       above loop and return - TBD */
    return (0);
}


static void setlog(const char *file)
{
    if (freopen(file, "w", stderr) == NULL) {
	ulog(WARN, "Unable to open log file %s (%s)", file,
	     strerror(errno));
    } else {
	ulog(INFO, "New log file created");
    }
}


static void usage(const char *path, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr,
	    "Usage: %s [options] -s key[:function] -l lib -q queue\n\n",
	    path);

    fprintf(stderr, "Options:\n"
	    "\t-s key[:function]     One or more binding keys (function names optional)\n"
	    "\t-U username           Username (default \"%s\")\n"
	    "\t-P password           Password (default \"%s\")\n"
	    "\t-h hostname           Broker host (defaults to current host)\n"
	    "\t-o filename           Write all output to the specified log file\n"
	    "\t-p port               Broker port (default %d)\n"
	    "\t-v vhost              Virtual host (default \"%s\")\n"
	    "\t-e exchange           Exchange name (default \"%s\")\n"
	    "\t-l filename           Shared library\n"
	    "\t-q queue              Queue name\n"
	    "\t-n count              Prefetch count\n"
	    "\t-D                    Don't declare queue or create bindings\n"
	    "\t-d                    Enable debug-level logging\n"
	    "\t-t                    Enable trace-level logging\n"
	    "\n"
	    "\tUse \"-s @filename\" to load service details from the specified file\n\n",
	    DEF_USER, DEF_PASSWORD, DEF_PORT, DEF_VHOST, DEF_EXCHANGE);

    exit(EXIT_FAILURE);
}


int main(int argc, char **argv)
{
    char tmp[128];
    int c;
    int n;
    amqp_rpc_reply_t rh;
    void *ip;
    int fd;
    int rv;

    int prefetch = 0;
    int port = DEF_PORT;
    int declare = 1;
    char *vhost = DEF_VHOST;
    char *log_file = NULL;
    char *host = NULL;
    char *user = DEF_USER;
    char *password = DEF_PASSWORD;
    char *shlib = NULL;

    gbl_t gbl = {
	0,
	DEF_EXCHANGE,
	NULL,
	NULL,
	NULL,
	NULL
    };


    assert((gbl.ht = adc_HT_New(HT_LEN, _hash, _match, _destroy)));

    n = 0;

    while ((c = getopt(argc, argv, "o:s:U:P:h:p:v:e:l:q:n:dtD")) != EOF) {
	switch (c) {
	case 's':
	    if (optarg[0] == '@') {
		n += load(gbl.ht, &optarg[1]);
	    } else {
		addkey(gbl.ht, optarg);
		n++;
	    }

	    break;

	case 'D':
	    declare = 0;
	    break;

	case 'U':
	    user = optarg;
	    break;

	case 'P':
	    password = optarg;
	    break;

	case 'h':
	    host = optarg;
	    break;

	case 'v':
	    vhost = optarg;
	    break;

	case 'e':
	    gbl.exchange = optarg;
	    break;

	case 'l':
	    shlib = optarg;
	    break;

	case 'q':
	    gbl.queue = optarg;
	    break;

	case 'o':
	    log_file = optarg;
	    break;

	case 'p':
	    port = atoi(optarg);
	    break;

	case 'd':
	    debug = 1;
	    break;

	case 't':
	    trace = 1;
	    break;

	case 'n':
	    prefetch = atoi(optarg);
	    break;

	default:
	    usage(argv[0], "Invalid command line option (-%c)\n", optopt);
	    break;
	}
    }


    if (log_file != NULL) {
	setlog(log_file);	/* If this fails we'll just log to stderr... */
    }

    if (n == 0) {
	usage(argv[0], "No routing keys (service details) specified\n");
    }

    if (shlib == NULL) {
	usage(argv[0], "No shared library specified\n");
    }

    if (gbl.queue == NULL) {
	usage(argv[0], "No queue name specified\n");
    }

    if (host == NULL) {
	if (gethostname(tmp, sizeof(tmp) - 1) == -1) {
	    ulog(FATAL, "gethostname(): %s", strerror(errno));
	}

	assert((host = strdup(tmp)));

	if (debug) {
	    ulog(INFO, "Using broker at %s:%d", host, port);
	}
    }


    /* Load shared library and determine function addresses */

    if ((ip = dlopen(shlib, RTLD_NOW)) == NULL) {
	ulog(FATAL, "dlopen(): %s", dlerror());
    }

    adc_HT_Traverse(gbl.ht, addsym, ip);


    /* See if we have an initialisation routine and a rundown routine */
    gbl.init = dlsym(ip, SVRINIT);
    gbl.done = dlsym(ip, SVRDONE);

    /* Call initialisation routine (if present) */
    if (gbl.init) {
	if (gbl.init(argc, argv) == -1) {
	    ulog(FATAL,
		 "Error status returned by user-supplied initialization routine - aborting");
	}
    }

    /* Time to start doing all the AMQP stuff... */
    if ((gbl.conn = amqp_new_connection()) == NULL) {
	ulog(FATAL, "Unable to allocate connection handle");
    }

    if ((fd = amqp_open_socket(host, port)) < 0) {
	ulog(FATAL, "Error opening socket: %s", amqp_error_string(-fd));
    }


    amqp_set_sockfd(gbl.conn, fd);

    rh = amqp_login(gbl.conn, vhost, 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
		    user, password);

    if (!OKAY(rh)) {
	ulog(FATAL, getmsg(rh, "Error logging in to broker"));
    }


    amqp_channel_open(gbl.conn, 1);

    rh = amqp_get_rpc_reply(gbl.conn);

    if (!OKAY(rh)) {
	ulog(FATAL, getmsg(rh, "Error opening channel"));
    }


    if (declare) {
	/* Declare queue */
	amqp_queue_declare(gbl.conn, 1, amqp_cstring_bytes(gbl.queue), 0,
			   0, 0, 1, amqp_empty_table);

	rh = amqp_get_rpc_reply(gbl.conn);

	if (!OKAY(rh)) {
	    ulog(FATAL, getmsg(rh, "Error declaring queue"));
	}

	/* Bind all routing keys to queue */
	adc_HT_Traverse(gbl.ht, bindkey, &gbl);
    }

    /* Set prefetch count if non-zero */
    if (prefetch != 0) {
	amqp_basic_qos(gbl.conn, 1, 0, prefetch, 0);

	rh = amqp_get_rpc_reply(gbl.conn);

	if (!OKAY(rh)) {
	    ulog(FATAL, getmsg(rh, "Error setting prefetch count"));
	}
    }

    /* Note that "noack" is "false", so we must acknowledge. While there is arguably little
       point doing an "ack" for an RPC, until we receive a message we don't know whether it
       is an RPC or not... so we always "ack" */

    amqp_basic_consume(gbl.conn, 1, amqp_cstring_bytes(gbl.queue),
		       amqp_empty_bytes, 0, 0, 0, amqp_empty_table);

    rh = amqp_get_rpc_reply(gbl.conn);

    if (!OKAY(rh)) {
	ulog(FATAL, getmsg(rh, "Unable to consume from queue"));
    }


    /* Start processing requests... */
    if (serve(&gbl) != 0) {
	ulog(INFO,
	     "Error status returned by user routine; server shutting down");
    }

    rh = amqp_channel_close(gbl.conn, 1, AMQP_REPLY_SUCCESS);

    if (!OKAY(rh)) {
	ulog(FATAL, getmsg(rh, "Error closing channel"));
    }

    rh = amqp_connection_close(gbl.conn, AMQP_REPLY_SUCCESS);

    if (!OKAY(rh)) {
	ulog(FATAL, getmsg(rh, "Error closing connection"));
    }

    if ((rv = amqp_destroy_connection(gbl.conn)) < 0) {
	ulog(FATAL, "Error destroying connection: %s",
	     amqp_error_string(-rv));
    }

// TBD - also need to see about calling gbl.done(), if it is defined!!

    if (ip != NULL) {
	dlclose(ip);		/* Not really any point in doing this, but what the heck... */
    }

    return (0);
}
