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
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <pthread.h>
#include <errno.h>
#include "npfs.h"
#include "npfsimpl.h"

typedef struct Fdtrans Fdtrans;

struct Fdtrans {
	Nptrans*	trans;
	int		connected;
	int 		fdin;
	int		fdout;
	struct pollfd*	pfdin;
	struct pollfd*	pfdout;
};

enum {
	TblModified	= 1,
	Notified	= 2,
};

typedef struct Nppoll Nppoll;
struct Nppoll {
	pthread_mutex_t	lock;
	int		shutdown;
	int		init;
	int		pipe[2];
	pthread_t	thread;
	int		flags;
	int		fdnum;
	int		fdsize;
	Fdtrans**	trans;
	struct pollfd*	fds;
};

Nppoll nppoll = {PTHREAD_MUTEX_INITIALIZER, 0 };

static void np_fdtrans_destroy(Nptrans *trans);
static void np_fdtrans_settbuf(Nptrans *trans);
static void np_fdtrans_setrbuf(Nptrans *trans);
static void poll_init(void);
static int poll_add(Fdtrans *trans, int fd, int events, struct pollfd **pfd);
static void poll_remove(Fdtrans *trans);
static void* poll_proc(void *a);
static void poll_notify(void);
static void np_fdtrans_read(Fdtrans *trans);
static void np_fdtrans_write(Fdtrans *trans);
static void np_fdtrans_error(Nptrans *trans);

Nptrans *
np_fdtrans_create(int fdin, int fdout)
{
	Nptrans *npt;
	Fdtrans *fdt;
	int n;

	fdt = malloc(sizeof(*fdt));
	npt = np_trans_create();
	fdt->trans = npt;
	fdt->connected = 0;
	fdt->fdin = fdin;
	fdt->fdout = fdout;
	fdt->connected = 1;
	npt->aux = fdt;
	npt->destroy = np_fdtrans_destroy;
	npt->settxbuf = np_fdtrans_settbuf;
	npt->setrxbuf = np_fdtrans_setrbuf;
	npt->error = np_fdtrans_error;

	fcntl(fdin, F_SETFL, O_NONBLOCK);
	if (fdin != fdout)
		fcntl(fdout, F_SETFL, O_NONBLOCK);

	if (fdin != fdout) {
		n = poll_add(fdt, fdin, 0, &fdt->pfdin);
		n &= poll_add(fdt, fdout, 0, &fdt->pfdout);
	} else {
		fdt->pfdout = NULL;
		n = poll_add(fdt, fdin, 0, &fdt->pfdin);
		fdt->pfdout = fdt->pfdin;
	}

	fdt->connected = n;

	return npt;
}

static void
np_fdtrans_destroy(Nptrans *trans)
{
	Fdtrans *fdt;

	fdt = trans->aux;
	poll_remove(fdt);
}

static void
np_fdtrans_settbuf(Nptrans *trans)
{
	Fdtrans *fdt;
	int n;

	fdt = trans->aux;
	if (trans->txbuf && trans->txbuf->buf) {
		fdt->pfdout->events |= POLLOUT;

		n = nppoll.flags & Notified;
		nppoll.flags |= Notified;
		if (!n)
			poll_notify();
	} else
		fdt->pfdout->events = fdt->pfdout->events & ~POLLOUT;
}

static void
np_fdtrans_setrbuf(Nptrans *trans)
{
	Fdtrans *fdt;
	int n;

	fdt = trans->aux;
	if (trans->rxbuf && trans->rxbuf->buf) {
		fdt->pfdin->events |= POLLIN;
		n = nppoll.flags & Notified;
		nppoll.flags |= Notified;
		if (!n)
			poll_notify();
	} else
		fdt->pfdout->events = fdt->pfdout->events & ~POLLIN;
}

static void
np_fdtrans_read(Fdtrans *fdt)
{
	int n;
	Npbuf *rbuf;
	Nptrans *trans;

	if (!fdt->connected)
		return;

	trans = fdt->trans;
	rbuf = trans->rxbuf;
	n = read(fdt->fdin, rbuf->buf + rbuf->pos, rbuf->size - rbuf->pos);
	if (n <= 0) {
		if (n != EAGAIN) {
			trans->connected = 0;
			(*rbuf->error)(rbuf->aux, n);
		}
	} else {
		rbuf->pos += n;
		(*rbuf->changed)(rbuf->aux);
	}
}

static void
np_fdtrans_write(Fdtrans *fdt)
{
	int n;
	Npbuf *wbuf;
	Nptrans *trans;

	if (!fdt->connected)
		return;

	trans = fdt->trans;
	wbuf = trans->txbuf;
	if (!wbuf || !wbuf->size)
		return;

	n = write(fdt->fdout, wbuf->buf+wbuf->pos, wbuf->size-wbuf->pos);
	if (n <= 0) {
		if (n != EAGAIN) {
			trans->connected = 0;
			if (n)
				n = errno;
			else
				n = EPIPE;
			(*wbuf->error)(wbuf->aux, n);
		}
	} else {		
		wbuf->pos += n;
		(*wbuf->changed)(wbuf->aux);
	}
}

static void
np_fdtrans_error(Nptrans *trans)
{
	if (trans->txbuf && trans->txbuf->error)
		trans->txbuf->error(trans->txbuf->aux, EPIPE);
}

static void
poll_init(void)
{
	nppoll.shutdown = 0;
	nppoll.fdnum = 1;
	nppoll.fdsize = 32;
	nppoll.trans = calloc(nppoll.fdsize, sizeof(Fdtrans *));
	nppoll.fds = calloc(nppoll.fdsize, sizeof(struct pollfd));
	pipe(nppoll.pipe);
	nppoll.flags = 0;

	nppoll.trans[0] = NULL;
	nppoll.fds[0].fd = nppoll.pipe[0];
	nppoll.fds[0].events = POLLIN | POLLOUT;
	nppoll.fds[0].revents = 0;

	pthread_create(&nppoll.thread, NULL, poll_proc, &nppoll);
	nppoll.init = 1;
}

static int
poll_add(Fdtrans *trans, int fd, int events, struct pollfd **ppfd)
{
	int i, ret;

	pthread_mutex_lock(&nppoll.lock);
	if (!nppoll.init) {
		poll_init();
	}

	for(i = nppoll.fdnum; i < nppoll.fdsize; i++)
		if (!nppoll.trans[i])
			break;

	if (i >= nppoll.fdsize)
		ret = 0;
	else {
		nppoll.trans[i] = trans;
		nppoll.fds[i].fd = fd;
		nppoll.fds[i].events = events | POLLERR | POLLHUP;
		*ppfd = &nppoll.fds[i];
		ret = 1;
	}

	i = nppoll.flags & Notified;
	nppoll.flags = TblModified | Notified;
	pthread_mutex_unlock(&nppoll.lock);
	if (!i)
		poll_notify();

	return ret;
}

static void
poll_remove(Fdtrans *trans)
{
	int i;

	if (!nppoll.init)
		return;

	pthread_mutex_lock(&nppoll.lock);
	for(i = 1; i < nppoll.fdnum; i++)
		if (nppoll.trans[i] == trans)
			nppoll.trans[i]->connected = 0;

	i = nppoll.flags & Notified;
	nppoll.flags = TblModified | Notified;
	pthread_mutex_unlock(&nppoll.lock);
	if (!i)
		poll_notify();
}

static void
poll_update_table(Nppoll *p)
{
	int i, n;
	struct pollfd *tfds;
	struct Fdtrans *fdt, **tfdt;

	for(i = n = 1; i < p->fdsize; i++) {
		fdt = p->trans[i];
		if (!fdt)
			continue;

		if (!fdt->connected) {
			close(fdt->fdin);
			if (fdt->fdin != fdt->fdout)
				close(fdt->fdout);
			free(fdt);
			p->trans[i] = NULL;
		} else {
			if (i != n) {
				p->trans[n] = p->trans[i];
				p->fds[n] = p->fds[i];
				if (p->fds[n].fd == p->trans[n]->fdin)
					p->trans[n]->pfdin = &p->fds[n];
				if (p->fds[n].fd == p->trans[n]->fdout)
					p->trans[n]->pfdout = &p->fds[n];

				p->trans[i] = NULL;
			}

			n++;
		}
	}

	p->fdnum = n;
	if (p->fdsize-p->fdnum < 32) {
		tfds = realloc(p->fds, sizeof(struct pollfd) * (p->fdsize+32));
		tfdt = realloc(p->trans, sizeof(Fdtrans *) * (p->fdsize+32));
		if (tfds && tfdt) {
			for(i = 0; i < 32; i++)
				tfdt[p->fdsize + i] = NULL;
			p->fdsize += 32;
			p->fds = tfds;
			p->trans = tfdt;

			for(i = 1; i < p->fdnum; i++) {
				if (p->fds[i].fd == p->trans[i]->fdin)
					p->trans[i]->pfdin = &p->fds[i];
				if (p->fds[i].fd == p->trans[i]->fdout)
					p->trans[i]->pfdout = &p->fds[i];
			}
		}
	}
}

static void*
poll_proc(void *a)
{
	int i, n, shutdown;
	Nppoll *p;
	struct pollfd *pfd;
	struct Fdtrans *fdt;
	char buf[10];

	p = a;
	shutdown = 0;
	while (!shutdown) {
		n = poll(p->fds, p->fdnum, 10000);
		if (p->fds[0].revents & POLLIN)
			n--;

		for(i = 1; i<p->fdnum && n; i++) {
			fdt = p->trans[i];
			pfd = &p->fds[i];
			if (pfd->revents)
				n--;

			if (!fdt)
				continue;

			if (pfd->revents & (POLLERR|POLLHUP|POLLNVAL)) {
				fdt->connected = 0;
				if (fdt->trans->error)
					(*fdt->trans->error)(fdt->trans);
				pfd->events = 0;
				continue;
			}

			if (pfd->revents & POLLIN) {
				np_fdtrans_read(fdt);
			}

			if (pfd->revents & POLLOUT) {
				np_fdtrans_write(fdt);
			}
		}

		if (p->fds[0].revents & POLLIN) {
			pthread_mutex_lock(&p->lock);
			shutdown = p->shutdown;
			n = read(p->pipe[0], buf, sizeof(buf));
			if (p->flags&TblModified)
				poll_update_table(p);
			p->flags = 0;
			pthread_mutex_unlock(&p->lock);
		}
	}

	return NULL;
}

static void
poll_notify(void)
{
	int n;
	char *buf = "";

	n = write(nppoll.pipe[1], buf, 1);
}
