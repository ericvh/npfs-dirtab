/*
  This file is a template to implement Dirtab based npfs fileservers.
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "npfs.h"
#include "casafs.h"
#include "myutils.h"
#include "myconsole.h"
#include "consolefs.h"

#define REPORT_ERROR(errcode) \
            {  \
	      create_rerror(errcode); \
	      goto done; \
	    } 


NpDtfileops defaultfileops = {
	.read = NULL,
	.write = NULL,
	.open = NULL
};


NpDtfileops consfileops = {
	.read = cons_read,
	.write = cons_write,
	.open = NULL
};


NpDtfileops timefileops = {
	.read = time_read,
	.write = NULL,
	.open = NULL
};

NpDtfileops msecfileops = {
	.read = msec_read,
	.write = NULL,
	.open = NULL
};

static Dirtab
consoletab[]={
  "dev",    Itopdir,  Qtdir, Nobody, &defaultfileops,
  "cons", Icons, Qtfile, Itopdir, &consfileops,
  "time", Itime, Qtfile, Itopdir, &timefileops,
  "msec", Imsec, Qtfile, Itopdir, &msecfileops
};

Npsrv *srv;
   
/* sets a qid structure according to a given dirtab entry  */
void dt2qid(Dirtab *dtentry, Npqid *qid, void *ignore)
{
  /* check if this is in the static part of the file tree  */
    qid->type = dtentry->qidtype;
    qid->version = 0; // just a dummy
    
    qid->path = dtentry->qidpath;  

}

/* sets the fid to correspond to a file in the filesystem directory table  */
void dt2fid(Dirtab *dtentry, Fid *fid, char *filename, void *ignore)
{
  dt2qid(dtentry, &(fid->qid), ignore);
  fid->filename = filename;
}	  




/* 
   read functions for various files  
   why do we pass the fid in ? because in clone file systems there neeeds
   to be a way of telling the clones apart
*/

Npfcall*
return_read(Npfcall *ret, int n)
{
	if (np_haserror()) {
		free(ret);
		ret = NULL;
	} else
		np_set_rread_count(ret, n);

	return ret;
}
  
Npfcall*
cons_read(Fid *f, u64 offset, u32 count, Npfcall *ret)
{
        int n;

        n = 0;
	if (fgets(ret->data, count, stdin) == NULL) 
	  REPORT_ERROR(errno);
	n = strlen(ret->data);
	if (n > count)
	  fprintf(stderr, "Warning: Buffer overflow in console read - number of bytes entered larger than buffer size\n");

 done:	
	return_read(ret, n);


 
}

Npfcall*
time_read(Fid *f, u64 offset, u32 count, Npfcall *ret)
{
        int n;
	char b[KNAMELEN];


	n = 0;
	snprintf(b, KNAMELEN, "%llu\n", getclock());
	if (offset > strlen(b)) 
	  goto done;
	if (strlen(b + offset) < count)
	  count = strlen(b + offset);
	strncpy(ret->data, b, count);
	n = count;

 done:	
	return_read(ret, n);

}

Npfcall*
msec_read(Fid *fid, u64 offset, u32 count, Npfcall *ret)
{
        int n;
	char b[KNAMELEN];
	u64 timemsec;

	n = 0;
	timemsec = msec();
	snprintf(b, KNAMELEN, "%llu\n", timemsec);
	if (offset > strlen(b)) 
	  goto done;
	if (strlen(b + offset) < count)
	  count = strlen(b + offset);
	strncpy(ret->data, b, count);
	n = count;

 done:
	return_read(ret, n);

}


/*
  write functions for the various files
*/

Npfcall*
return_write(int n)
{
       if (np_haserror()) 
          return NULL;
       else
          return np_create_rwrite(n);
}

Npfcall*
cons_write(Fid *fid, u64 offset, u32 count, u8 *data, Npreq *req)
{
	int n;

	n = 0;
	while (count > 0) {
	    putchar((int)data[n]);
	    n++; count--;
	}
 done:
	return_write(n);
}


void
usage()
{
  fprintf(stderr, "Usage: consolefs [-p port] [-m path to mount point] [-l logfile]\n");
  fprintf(stderr, "Info:\n -p port = makes this a socket based server and assigns port where it listens\n");
  fprintf(stderr, " -m path = mounts this server locally at given path (default = exported over socket)\n");
  fprintf(stderr, " -l logfile = logs incoming requests to given file (default = printed to console)\n");

  exit(-1);
}


int
main(int argc, char **argv)
{
	int c; 
	int port, nwthreads;
	char *s;
	char *logfile;
	int issocket;
	char *opts;
	int fd;
	char *mountpath;

	port = 2001;
	nwthreads = 1;
	issocket = 1; /* default server type is socket based  */
	opts = NULL;
	logfile = NULL;
	
	while ((c = getopt(argc, argv, "dsl:p:w:o:m:")) != -1) {
		switch (c) {
		case 'd':
			break;

		case 'p':
			port = strtol(optarg, &s, 10);
			if (*s != '\0')
				usage();
			break;

		case 'w':
			nwthreads = strtol(optarg, &s, 10);
			if (*s != '\0')
				usage();
			break;

		case 's':
			break;

		case 'm':
		        issocket = 0;
			mountpath = optarg;
			break;	
		case 'l':
		        logfile = optarg;
  		        break;
		default:
			usage();
		}
	}

	if (logfile != NULL) {
	  fd = open(logfile, O_WRONLY | O_APPEND | O_CREAT, 0666);
	  if (fd < 0) {
	    fprintf(stderr, "cannot open log file %s: %d\n", logfile, errno);
	    return -1;
	  }
	  close(2);
	  if (dup2(fd, 2) < 0) {
	    printf("dup failed: %d\n", errno);
	    return -1;
	  }
	}
	

	if (issocket == 1)
	  srv = np_socksrv_create_tcp(nwthreads, &port);
	else 
	  srv = np_pipesrv_create(nwthreads);

	if (!srv)
		return -1;

	npfile_init_dirtab(srv, consoletab, NELEM(consoletab));

	if (issocket == 1)
	  np_srv_start(srv);
	else
	  if (np_pipesrv_mount(srv, mountpath, "guest", 0, opts))
	    return -1;

	while (1) {
		sleep(100);
	}

	return 0;
}
