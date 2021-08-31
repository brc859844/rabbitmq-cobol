#ifndef __RMQ_H__
#define __RMQ_H__

#ifdef __VMS
#include <inttypes.h>
#else
#include <stdint.h>
#endif
#include <stdlib.h>

#include "amqp.h"
#ifdef __VMS
#ifdef AMQP091
#include "amqp_framing_091.h"
#else
#include "amqp_framing_080.h"
#endif
#else
#include "amqp_framing.h"
#endif

#ifndef RMQ_MAX_FRAME
#if defined(__VMS) && defined(__alpha) && __CRTL_VER <= 70320000
/* Assume that for OpenVMS 7.3-2 and earlier these need to be < 64K to stop send() and recv() having a fit! */
#define RMQ_MAX_FRAME 65535
#else
#define RMQ_MAX_FRAME 131072
#endif
#endif

#ifndef RMQ_MAX_CHAN
#define RMQ_MAX_CHAN 256
#endif

#ifndef RMQ_Q_NAM_LEN
#define RMQ_Q_NAM_LEN 128
#endif

typedef struct {
    amqp_connection_state_t conn;
    int chan;			/* Will always be 1 (for the moment) */
    int fd;
    char errstr[128];
    struct {
	char name[RMQ_Q_NAM_LEN];
	amqp_bytes_t repq;
	amqp_basic_properties_t *ph;
	long long count;
    } rpc;
} RMQ_conn_t;


typedef struct {
    char *rkey;
    char *repq;
    uint64_t dtag;
    char *cid;
    amqp_bytes_t data;
} RMQ_info_t;



#ifdef __cplusplus
extern "C" {
#endif

    extern char *RabbitMQ_strerror(RMQ_conn_t *);
    extern RMQ_conn_t *RabbitMQ_connect(char *);
    extern void RabbitMQ_disconnect(RMQ_conn_t *);
    extern int RabbitMQ_publish(RMQ_conn_t *, char *, char *, int, int,
				amqp_basic_properties_t *, char *, int);
    extern char *RabbitMQ_declare_queue(RMQ_conn_t *, char *, int, int,
					int, int, amqp_table_t *);
    extern int RabbitMQ_declare_exchange(RMQ_conn_t *, char *, char *, int,
					 int, amqp_table_t *);
    extern int RabbitMQ_bind_queue(RMQ_conn_t *, char *, char *, char *,
				   amqp_table_t *);
    extern int RabbitMQ_bind_exchange(RMQ_conn_t *, char *, char *, char *,
				      amqp_table_t *);
    extern char *RabbitMQ_consume(RMQ_conn_t *, char *, char *, int, int,
				  int, amqp_table_t *);
    extern int RabbitMQ_dequeue(RMQ_conn_t *, RMQ_info_t *, uint64_t *,
				int);
    extern void RabbitMQ_dump(char *, int);
    extern void RabbitMQ_free_info(RMQ_info_t *);
    extern int RabbitMQ_serve(RMQ_conn_t *, int (*)(RMQ_info_t *, void *),
			      int, void *, int);
    extern void RabbitMQ_info_init(RMQ_info_t *);
    extern int RabbitMQ_purge_queue(RMQ_conn_t *, char *);
    extern int RabbitMQ_delete_queue(RMQ_conn_t *, char *, int, int);
    extern int RabbitMQ_delete_exchange(RMQ_conn_t *, char *, int);
    extern int RabbitMQ_unbind_queue(RMQ_conn_t *, char *, char *, char *,
				     amqp_table_t *);
    extern int RabbitMQ_unbind_exchange(RMQ_conn_t *, char *, char *,
					char *, amqp_table_t *);
    extern int RabbitMQ_rpc_call(RMQ_conn_t *, char *, char *, char *, int,
				 RMQ_info_t *);
    extern int RabbitMQ_qos(RMQ_conn_t *, int, int, int);
    extern int RabbitMQ_get(RMQ_conn_t *, const char *, RMQ_info_t *, int);
    extern RMQ_info_t *RabbitMQ_alloc_info();
    extern int RabbitMQ_tx_select(RMQ_conn_t *);
    extern int RabbitMQ_tx_commit(RMQ_conn_t *);
    extern int RabbitMQ_tx_rollback(RMQ_conn_t *);
    extern int RabbitMQ_cancel(RMQ_conn_t *, char *);
    extern void RabbitMQ_release(RMQ_conn_t *);

#ifndef _WIN32
#ifdef __VMS
#pragma names save
#pragma names uppercase
#endif
#include <pthread.h>
#ifdef __VMS
#pragma names restore
#endif
    extern pthread_t RabbitMQ_serve_thread(RMQ_conn_t *,
					   int (*)(RMQ_info_t *, void *),
					   int, void *, int);
#endif

#ifdef __cplusplus
}
#endif
#if defined __GNUC__
#define likely(x) __builtin_expect ((x), 1)
#define unlikely(x) __builtin_expect ((x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#define RMQ_Assert(x) \
    do { \
        if (unlikely (!x)) { \
            fprintf (stderr, "Assertion failed: %s (%s: %d)\n", #x, __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)
#define RMQ_AllocAssert(x) \
    do { \
        if (unlikely (!x)) { \
            fprintf (stderr, "FATAL ERROR: OUT OF MEMORY (%s: %d)\n", __FILE__, __LINE__); \
            abort(); \
        } \
    } while (0)
#define RMQ_Free(x) \
    do { \
        if (x != NULL) { \
            free(x); x = NULL; \
        } \
    } while (0)
#endif
