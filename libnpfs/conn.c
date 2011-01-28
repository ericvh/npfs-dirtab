/*
 * Copyright (C) 2005 by Latchesar Ionkov <lucho@ionkov.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "npfs.h"
#include "npfsimpl.h"

extern int printfcall(FILE *f, Npfcall *fc, int dotu);
static void np_conn_error(Npconn *conn, int err);
static void np_conn_data_in(void *);
static void np_conn_data_out(void *);
static void np_conn_call_in(Npconn *, Npfcall *);
static void np_conn_new_rcall(Npconn *);
static void np_conn_new_wcall(Npconn *, Npfcall *);
static void np_buf_init(Npbuf *, void *, void (*)(void *),
	void (*)(void *, int));
static void np_buf_set(Npbuf *, u8 *, u32);

Npconn*
np_conn_create(Npsrv *srv, Nptrans *trans)
{
	Npconn *conn;

	conn = malloc(sizeof(*conn));
	if (!conn)
		return NULL;

	pthread_mutex_init(&conn->lock, NULL);
	conn->srv = srv;
	conn->msize = srv->msize;
	conn->dotu = srv->dotu;
	conn->shutdown = 0;
	conn->fidpool = np_fidpool_create();
	conn->trans = trans;
	np_buf_init(&conn->rbuf, conn, np_conn_data_in, 
		(void(*)(void *, int)) np_conn_error);
	np_buf_init(&conn->wbuf, conn, np_conn_data_out, 
		(void(*)(void *, int)) np_conn_error);
	conn->rcalls = NULL;
	conn->rcallp = &conn->rcalls;
	conn->aux = NULL;
	conn->wcall = NULL;
	conn->rcall = NULL;
	conn->freercnum = 0;
	conn->freerclist = NULL;
	np_conn_new_rcall(conn);
	np_trans_set_rxbuf(conn->trans, &conn->rbuf);
	np_conn_new_wcall(conn, NULL);

	return conn;
}

void
np_conn_reset(Npconn *conn, u32 msize, int dotu)
{
	Npsrv *srv;
	Npreq *req, *req1, **prevp;
	Npfcall *rc, *rc1;

	pthread_mutex_lock(&conn->srv->lock);
	srv = conn->srv;
	// first flush all outstanding requests
	req = srv->reqs_first;
	while (req != NULL) {
		if (req->conn == conn) {
			req1 = req->next;
			np_srv_remove_request(srv, req);
			free(req->tcall);
			free(req);
			req = req1;
		} else {
			prevp = &req->next;
			req = *prevp;
		}
	}

	// then mark all working requests as cancelled
	req = conn->srv->workreqs;
	while (req != NULL) {
		if (req->conn == conn && req->tcall->id != Tversion) {
			req->cancelled = 1;
		}
		req = req->next;
	}
	pthread_mutex_unlock(&conn->srv->lock);

	pthread_mutex_lock(&conn->lock);
	// don't send any responses queued for sending
	rc = conn->rcalls;
	conn->rcalls = NULL;
	while (rc != NULL) {
		rc1 = rc->next;
		free(rc);
		rc = rc->next;
	}

	// free all rcall from the pool
	rc = conn->freerclist;
	while (rc != NULL) {
		rc1 = rc->next;
		free(rc);
		rc = rc1;
	}
	conn->freercnum = 0;
	conn->freerclist = NULL;

	// TODO: wait until the transport finishes sending (if it does)
	if (!conn->shutdown) {
		conn->msize = msize;
		conn->dotu = dotu;
		free(conn->rcall);
		np_conn_new_rcall(conn);
		np_trans_set_rxbuf(conn->trans, &conn->rbuf);
		np_conn_new_wcall(conn, NULL);
		pthread_mutex_unlock(&conn->lock);
	} else {
		free(conn->rcall);
		free(conn->wcall);
	}
}

void
np_conn_shutdown(Npconn *conn, int reset)
{
	np_srv_remove_conn(conn->srv, conn);

	pthread_mutex_lock(&conn->lock);
	conn->shutdown = 1;
	np_trans_destroy(conn->trans);
	pthread_mutex_unlock(&conn->lock);

	if (reset)
		np_conn_reset(conn, conn->srv->msize, 0);
}

static void
np_conn_error(Npconn *conn, int err)
{
	np_conn_shutdown(conn, 1);
}

static void
np_conn_data_in(void *a)
{
	int n, rpos, bufchanged;
	Npconn *conn;
	Npbuf *rb;
	Npfcall *rcall;
	u8 *rbuf;

	conn = a;
	rb = &conn->rbuf;

	bufchanged = 0;
	while (rb->pos > 4) {
		n = rb->buf[0] | (rb->buf[1]<<8) | (rb->buf[2]<<16) | 
			(rb->buf[3]<<24);

		if (n > rb->size) {
			np_conn_error(conn, ENOMEM);
			return;
		}

		if (rb->pos < n)
			break;

		n = np_deserialize(conn->rcall, rb->buf, conn->dotu);
		if (conn->srv->debuglevel) {
			fprintf(stderr, "<<< ");
			printfcall(stderr, conn->rcall, conn->dotu);
			fprintf(stderr, "\n");
		}

		if (!n) {
			np_conn_error(conn, EPROTO);
			return;
		}

		rcall = conn->rcall;
		rbuf = rb->buf;
		rpos = rb->pos;
		pthread_mutex_lock(&conn->lock);
		np_conn_new_rcall(conn);
		pthread_mutex_unlock(&conn->lock);
		if (rpos > n)
			memmove(conn->rbuf.buf, rbuf + n, rpos - n);

		conn->rbuf.pos = rpos-n;

		np_conn_call_in(conn, rcall);
		bufchanged = 1;
	}

	if (bufchanged)
		np_trans_set_rxbuf(conn->trans, &conn->rbuf);
}

static void
np_conn_data_out(void *a)
{
	Npbuf *wb;
	Npfcall *rc;
	Npconn *conn;

	conn = a;
	wb = &conn->wbuf;
	if (wb->pos < wb->size)
		return;

	pthread_mutex_lock(&conn->lock);
	rc = conn->rcalls;
	if (rc) {
		conn->rcalls = rc->next;
		if (conn->rcalls == NULL)
			conn->rcallp = &conn->rcalls;
	}

	np_conn_new_wcall(conn, rc);
	pthread_mutex_unlock(&conn->lock);
	np_trans_set_txbuf(conn->trans, &conn->wbuf);
}

static void
np_conn_call_in(Npconn *conn, Npfcall *tc)
{
	Npsrv *srv;
	Npreq *req;

	req = reqalloc();
	req->conn = conn;
	req->tag = tc->tag;
	req->tcall = tc;

	pthread_mutex_lock(&conn->srv->lock);
	srv = conn->srv;
	req->prev = srv->reqs_last;
	if (srv->reqs_last)
		srv->reqs_last->next = req;
	srv->reqs_last = req;
	if (!srv->reqs_first)
		srv->reqs_first = req;

	pthread_mutex_unlock(&conn->srv->lock);
	pthread_cond_signal(&conn->srv->reqcond);
}

void
np_conn_send_fcall(Npconn *conn, Npfcall *rc)
{
	if (conn->srv->debuglevel) {
		fprintf(stderr, ">>> ");
		printfcall(stderr, rc, conn->dotu);
		fprintf(stderr, "\n");
	}

	pthread_mutex_lock(&conn->lock);
	if (!conn->wbuf.size) {
		np_conn_new_wcall(conn, rc);
		pthread_mutex_unlock(&conn->lock);
		np_trans_set_txbuf(conn->trans, &conn->wbuf);
	} else {
		rc->next = NULL;
		*conn->rcallp = rc;
		conn->rcallp = &rc->next;
		pthread_mutex_unlock(&conn->lock);
	}
}

static void
np_conn_new_rcall(Npconn *conn)
{
	Npfcall *rc;

	if (conn->freerclist) {
		rc = conn->freerclist;
		conn->freerclist = rc->next;
		conn->freercnum--;
	} else {
		rc = malloc(sizeof(*rc) + conn->msize);
	}

	rc->pkt = (u8*) rc + sizeof(*rc);
	conn->rcall = rc;
	np_buf_set(&conn->rbuf, rc->pkt, conn->msize);
}

void
np_conn_free_rcall(Npconn* conn, Npfcall *rc)
{
	pthread_mutex_lock(&conn->lock);
	if (conn->freercnum < 64) {
		rc->next = conn->freerclist;
		conn->freerclist = rc;
		rc = NULL;
	}
	pthread_mutex_unlock(&conn->lock);

	if (rc)
		free(rc);
}

static void
np_conn_new_wcall(Npconn *conn, Npfcall *wc)
{
	u32 size;
	u8* buf;

	free(conn->wcall);
	conn->wcall = wc;
	if (wc) {
		size = wc->size;
		buf = wc->pkt;
	} else {
		size = 0;
		buf = NULL;
	}
	np_buf_set(&conn->wbuf, buf, size);
}

static void
np_buf_init(Npbuf *buf, void *aux, void (*changed)(void *),
	void (*error)(void *, int))
{
	pthread_mutex_init(&buf->lock, NULL);
	buf->size = 0;
	buf->pos = 0;
	buf->buf = NULL;
	buf->aux = aux;
	buf->changed = changed;
	buf->error = error;
}

static void
np_buf_set(Npbuf *buf, u8 *data, u32 size)
{
	buf->pos = 0;
	buf->size = size;
	buf->buf = data;
}

Nptrans *
np_trans_create()
{
	Nptrans *trans;

	trans = malloc(sizeof(*trans));
	pthread_mutex_init(&trans->lock, NULL);
	trans->txbuf = NULL;
	trans->rxbuf = NULL;

	return trans;
}

void
np_trans_destroy(Nptrans *trans)
{
	(*trans->destroy)(trans);
	free(trans);
}

void
np_trans_set_txbuf(Nptrans *trans, Npbuf *buf)
{
	pthread_mutex_lock(&trans->lock);
	trans->txbuf = buf;
	if (trans->settxbuf)
		(*trans->settxbuf)(trans);
	pthread_mutex_unlock(&trans->lock);
}

void
np_trans_set_rxbuf(Nptrans *trans, Npbuf *buf)
{
	pthread_mutex_lock(&trans->lock);
	trans->rxbuf = buf;
	if (trans->setrxbuf)
		(*trans->setrxbuf)(trans);
	pthread_mutex_unlock(&trans->lock);
}
