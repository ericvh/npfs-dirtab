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
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include "npfs.h"
#include "npfsimpl.h"

typedef struct Socksrv Socksrv;

struct Socksrv {
	int			sock;
	int			shutdown;
	struct sockaddr*	addr;
	pthread_t		listenproc;
};

static void np_socksrv_start(Npsrv *srv);
static void np_socksrv_shutdown(Npsrv *srv);
static void np_socksrv_destroy(Npsrv *srv);
static void * np_socksrv_listenproc(void *a);

static Socksrv*
np_socksrv_create_common(int domain, int type, int proto)
{
	Socksrv *ss;

	ss = malloc(sizeof(*ss));
	ss->shutdown = 0;
	ss->sock = socket(domain, type, proto);
	if (ss->sock < 0) {
		fprintf(stderr, "cannot create socket: %d\n", errno);
		free(ss);
		return NULL;
	}

	return ss;
}

Npsrv*
np_socksrv_create_tcp(int nwthreads, int *port)
{
	socklen_t n;
	Npsrv *srv;
	Socksrv *ss;
	int mysocket;
	struct sockaddr_in* saddr;

        ss = np_socksrv_create_common(PF_INET, SOCK_STREAM, 0);
	ss = np_socksrv_create_common(AF_INET, SOCK_STREAM, 0);
	//	my_socket=socket(AF_INET,SOCK_STREAM,0);

	saddr = malloc(sizeof(*saddr));
	ss->addr = (struct sockaddr *) saddr;

	saddr->sin_family = AF_INET;
	saddr->sin_port = htons(*port);
	//saddr->sin_port = htons(2001);
	saddr->sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(ss->sock, ss->addr, sizeof(*saddr)) < 0) {
		fprintf(stderr, "cannot bind: %d\n", errno);
		perror("bind");
		free(ss);
		return NULL;
	}

	saddr->sin_port = 4242;
	n = sizeof(*saddr);
	if (getsockname(ss->sock, ss->addr, &n) < 0) {
		perror("getsockname");
	}

	*port = ntohs(saddr->sin_port);

	srv = np_srv_create(nwthreads);
	srv->srvaux = ss;
	srv->start = np_socksrv_start;
	srv->shutdown = np_socksrv_shutdown;
	srv->destroy = np_socksrv_destroy;

	return srv;
}

/*
int
np_socksrv_get_port(Npsrv *srv)
{
	Socksrv *ss;

	ss = srv->srvaux;
	return ss->addr->port;
}
*/

static void
np_socksrv_start(Npsrv *srv)
{
	Socksrv *ss;

	ss = srv->srvaux;
	pthread_create(&ss->listenproc, NULL, np_socksrv_listenproc, srv);
}

static void
np_socksrv_shutdown(Npsrv *srv)
{
	Socksrv *ss;

	ss = srv->srvaux;
	ss->shutdown = 1;
	close(ss->sock);
}

static void
np_socksrv_destroy(Npsrv *srv)
{
	Socksrv *ss;
	void *ret;

	ss = srv->srvaux;
	pthread_join(ss->listenproc, &ret);
	free(ss);
	srv->srvaux = NULL;
}

static void *
np_socksrv_listenproc(void *a)
{
	int csock;
	Npsrv *srv;
	Socksrv *ss;
	struct sockaddr_in caddr;
	socklen_t caddrlen;
	Npconn *conn;
	Nptrans *trans;

	srv = a;
	ss = srv->srvaux;


	if (listen(ss->sock, 1) < 0)
		return NULL;

	while (!ss->shutdown) {
		caddrlen = sizeof(caddr);
		csock = accept(ss->sock, (struct sockaddr *) &caddr, &caddrlen);
		if (csock<0) {
			if (!ss->shutdown)
				fprintf(stderr, "error while accepting: %d\n", errno);

			continue;
		}

		trans = np_fdtrans_create(csock, csock);
		conn = np_conn_create(srv, trans);
		np_srv_add_conn(srv, conn);
	}

	return NULL;
}
