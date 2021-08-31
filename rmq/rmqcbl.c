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
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "rmq.h"

#ifndef __VMS
#ifndef AMQP091
#define AMQP091
#endif
#endif				// __VMS



static char *rtrim(char *str)
{
    char *cp;

    cp = &str[strlen(str) - 1];

    while (isspace((int) *cp)) {
	*cp = '\0';
	cp--;
    }

    return (str);
}


static char *mkstr(char *str, int len)
{
    char *tmp = NULL;

    if (len == 0) {
	return (strdup(str));
    } else {
	if ((tmp = (char *) calloc(1, (len + 1) * sizeof(char))) == NULL) {
	    return (NULL);
	} else {
	    memcpy(tmp, str, len);
	    tmp[len] = '\0';

	    return (rtrim(tmp));
	}
    }
}


void RMQ_STRERROR(void *handle, char *str, int len)
{
    RMQ_conn_t *ch = (RMQ_conn_t *) handle;
    int n;
    char *tmp;

    assert(str);

    tmp = RabbitMQ_strerror(ch);

    if ((n = strlen(tmp)) > len) {
	n = len;
    }

    memset(str, ' ', len);
    memcpy(str, tmp, n);
}


int RMQ_CONNECT(void **handle, char *url, int len)
{
    char *tmp;

    assert(handle);
    assert(url);

    tmp = mkstr(url, len);
    *handle = (void *) RabbitMQ_connect(tmp);
    free(tmp);

    return ((*handle) ? 1 : 0);
}


void RMQ_DISCONNECT(void *handle)
{
    assert(handle);
    RabbitMQ_disconnect((RMQ_conn_t *) handle);
}


int
RMQ_PUBLISH(void *handle, char *exch, int exch_len, char *rkey,
	    int rkey_len, int mand, int immed, char *body, int len,
	    void *props)
{
    char *exch_tmp;
    char *rkey_tmp;
    int rv;

    assert(handle);
    assert(exch);
    assert(rkey);
    assert(body);

    exch_tmp = mkstr(exch, exch_len);
    rkey_tmp = mkstr(rkey, rkey_len);

    rv = RabbitMQ_publish((RMQ_conn_t *) handle, exch_tmp, rkey_tmp, mand,
			  immed, props, body, len);
    free(exch_tmp);
    free(rkey_tmp);

    return (rv == -1 ? 0 : 1);
}


void RMQ_DUMP(char *addr, int len)
{
    assert(addr);
    RabbitMQ_dump(addr, len);
}


int RMQ_ALLOC(void **info)
{
    assert(info);

    if ((*info = (RMQ_info_t *) RabbitMQ_alloc_info()) != NULL) {
	return (1);
    } else {
	return (0);
    }
}


int
RMQ_RPC_CALL(void *handle, char *exch, int exch_len, char *rkey,
	     int rkey_len, char *rqst, int rqst_len, void *repl,
	     int *repl_len)
{
    char *exch_tmp;
    char *rkey_tmp;
    int rv;
    RMQ_info_t data;

    assert(handle);
    assert(exch);
    assert(rkey);
    assert(rqst);
    assert(repl);
    assert(repl_len);

    memset(&data, '\0', sizeof(data));

    exch_tmp = mkstr(exch, exch_len);
    rkey_tmp = mkstr(rkey, rkey_len);

    rv = RabbitMQ_rpc_call((RMQ_conn_t *) handle, exch_tmp, rkey_tmp, rqst,
			   rqst_len, &data);

    free(exch_tmp);
    free(rkey_tmp);

    if (rv == -1) {
	return (0);
    }

    if (data.data.len > *repl_len) {
	// Problem (TBD)

	return (0);
    }


    memset(repl, '\0', *repl_len);	// Why '\0' ?? (TBD)
    memcpy(repl, data.data.bytes, data.data.len);
    *repl_len = data.data.len;

    RabbitMQ_info_init(&data);

    return (1);
}


int
RMQ_DECLARE_QUEUE(void *handle, char *i_nam, int ilen, char *o_nam,
		  int *olen, int passive, int durable, int exclsve,
		  int autodel, void *args)
{
    char *name_tmp = NULL;
    char *tmp;
    size_t len;

    assert(handle);

    if (i_nam && ilen) {
	name_tmp = mkstr(i_nam, ilen);
    }

    tmp =
	RabbitMQ_declare_queue((RMQ_conn_t *) handle, name_tmp, passive,
			       durable, exclsve, autodel,
			       (amqp_table_t *) args);

    if (name_tmp) {
	free(name_tmp);
    }

    if (tmp == NULL) {
	return (0);
    }

    /* See if we have somewhere to stick the queue name */
    if (o_nam && olen && *olen) {
	if ((len = strlen(tmp)) > *olen) {
	    // Problem (TBD)

	    return (0);
	}

	memset(o_nam, ' ', *olen);
	memcpy(o_nam, tmp, len);
	*olen = len;
    }

    return (1);
}


int
RMQ_BIND_QUEUE(void *handle, char *qnam, int qnam_len,
	       char *exch, int exch_len,
	       char *bkey, int bkey_len, void *args)
{
    char *qnam_tmp = NULL;
    char *exch_tmp = NULL;
    char *bkey_tmp = NULL;
    int rv;

    assert(handle);
    assert(qnam);
    assert(exch);
    assert(bkey);

    qnam_tmp = mkstr(qnam, qnam_len);
    exch_tmp = mkstr(exch, exch_len);
    bkey_tmp = mkstr(bkey, bkey_len);

    rv = RabbitMQ_bind_queue((RMQ_conn_t *) handle, qnam_tmp, exch_tmp,
			     bkey_tmp, (amqp_table_t *) args);

    free(qnam_tmp);
    free(exch_tmp);
    free(bkey_tmp);

    return (rv == -1 ? 0 : 1);
}


int
RMQ_DECLARE_EXCHANGE(void *handle, char *name, int name_len,
		     char *type, int type_len, int passive, int durable,
		     void *args)
{
    char *name_tmp = NULL;
    char *type_tmp = NULL;
    int rv;

    assert(handle);
    assert(name);
    assert(type);

    name_tmp = mkstr(name, name_len);
    type_tmp = mkstr(type, type_len);

    rv = RabbitMQ_declare_exchange((RMQ_conn_t *) handle, name_tmp,
				   type_tmp, passive, durable,
				   (amqp_table_t *) args);

    free(name_tmp);
    free(type_tmp);

    return (rv == -1 ? 0 : 1);
}


void *RMQ_MSG_PROPS_PERSISTENT()
{
    static const amqp_basic_properties_t MESSAGE_PROPERTIES_PERSISTENT = {
	AMQP_BASIC_DELIVERY_MODE_FLAG,
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	2,			/* Persistent */
	0,
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	0,
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL },
	{ 0, NULL }
    };

    return ((void *) &MESSAGE_PROPERTIES_PERSISTENT);
}


void *RMQ_PROPS_NEW()
{
    amqp_basic_properties_t *props;
    props =
	(amqp_basic_properties_t *) calloc(1,
					   sizeof
					   (amqp_basic_properties_t));
    return (props);
}


void RMQ_PROPS_FREE(void *pp)
{
    free(pp);
}


void RMQ_PROPS_SET(void *pp, int opt, void *val, int size)
{
    amqp_basic_properties_t *props = (amqp_basic_properties_t *) pp;
    amqp_bytes_t dtm;

    switch (opt) {
    case AMQP_BASIC_CONTENT_TYPE_FLAG:
	props->_flags |= opt;
	dtm.len = size;
	dtm.bytes = val;
	props->content_type = dtm;

	break;

    case AMQP_BASIC_DELIVERY_MODE_FLAG:
	props->_flags |= opt;
	props->delivery_mode = *(uint8_t *) val;

	break;

    default:
	/* TBD - should report something */
	break;
    }
}
