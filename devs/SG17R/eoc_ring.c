/*
#include <linux/slab.h>
#include <asm/types.h>
#include <asm/byteorder.h>
*/
#include <stdio.h>
#include <errno.h>

#include "ring_buf.h"

#define free(ptr) free(ptr)
#define malloc(size) malloc(size)

int inc_index(int ind){
    if( ++ind < RING_SIZE )
	return ind;
    return 0;
}

void rbuf_free_buf(struct rbuf_buf *cur)
{
    int i;
    for(i=0;(i<cur->used && i< MAX_SBUFS);i++){
	free(cur->ptr[i]);
    }
    cur->used = 0;
}

int rbuf_count_size(struct rbuf_buf *cur)
{
    int i;
    int size = 0;
    for(i=0;(i<cur->used && i< MAX_SBUFS);i++){
	    size += cur->size[i];
    }
    return size;
}


struct ring_buf *
rbuf_init(void)
{
    struct ring_buf *r = (struct ring_buf *)malloc(sizeof(struct ring_buf));
    int i;

    if( !r )
	return NULL;
    r->head = r->tail = 0;
    for(i=0;i<RING_SIZE;i++){
	r->rb[i].used = 0;
    }
    return r;
}


void rbuf_free(struct ring_buf *r)
{
    int i;
    for(i=0;i<RING_SIZE;i++){
	rbuf_free_buf(&r->rb[i]);
    }
    free(r);
}


int rbuf_append(struct ring_buf *r,char *in_buf,int size,int end_flag)
{
    struct rbuf_buf *cur;

    if( inc_index(r->tail) != r->head ){
	cur = &(r->rb[r->tail]);
	if( cur->used == MAX_SBUFS ){
	    rbuf_free_buf(cur);
	    return -ENOMEM;
	}
	if( !(cur->ptr[cur->used] = malloc(size)) )
	    return -ENOMEM;
	memcpy(cur->ptr[cur->used],in_buf,size);
	cur->size[cur->used] = size;
	cur->used++;
	if( end_flag )
	    r->tail = inc_index(r->tail);
	return 0;
    }
    return -1;
}

void rbuf_abort_cur(struct ring_buf *r)
{
    struct rbuf_buf *cur;
    if( inc_index(r->tail) != r->head ){
	cur = &(r->rb[r->tail]);
	rbuf_free_buf(&r->rb[r->head]);
    }
}


int
rbuf_get_cur(struct ring_buf *r,char **out_buf)
{
    struct rbuf_buf *cur;
    int size,offset = 0;
    int i;
    if( r->head != r->tail ){
	cur = &(r->rb[r->head]);
	size = rbuf_count_size(cur);
	if( !(*out_buf = malloc(size)) ){
	    return -1;
	}
	offset = 0;
	for(i=0;i<cur->used;i++){
	    memcpy(*out_buf+offset,cur->ptr[i],cur->size[i]);
	    offset += cur->size[i];
	}
	rbuf_free_buf(&r->rb[r->head]);
	r->head = inc_index(r->head);
	return size;
    }
    return -1;
}

