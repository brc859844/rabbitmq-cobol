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

#ifndef __GSHASH_H__
#define __GSHASH_H__

#include "list.h"

typedef struct {
    int buckets;
    unsigned int (*hash) (const void *);
    int (*match) (const void *, const void *);
    void (*destroy) (void *);
    int size;
    adc_SLL_t *table;
} adc_HT_t;


#ifdef __cplusplus
extern "C" {
#endif

    extern int adc_HT_Init(adc_HT_t *, int, unsigned int (*)(const void *),
			   int (*)(const void *, const void *),
			   void (*)(void *)
	);
    extern adc_HT_t *adc_HT_New(int, unsigned int (*)(const void *),
				int (*)(const void *, const void *),
				void (*)(void *));

    extern void adc_HT_Destroy(adc_HT_t *);
    extern int adc_HT_Insert(adc_HT_t *, const void *);
    extern int adc_HT_Remove(adc_HT_t *, void **);
    extern int adc_HT_Lookup(const adc_HT_t *, void **);
    extern int adc_HT_Exists(const adc_HT_t *, void *);
    extern void adc_HT_Traverse(const adc_HT_t *,
				void (*)(const void *, void *), void *);
    extern void adc_HT_Reset(adc_HT_t *);

#define adc_HT_Size(htbl) ((htbl)->size)

#ifdef __cplusplus
}
#endif
#endif
