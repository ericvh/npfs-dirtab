/*
  This file implements a /net type filesystem using npfs.
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include "npfs.h"
#include "casafs.h"
#include "myconn.h"
#include "netfs.h"
#include "myutils.h"
#include "TransferPoint.h"


Npsrv *srv;
int debuglevel;
int sameuser;

#define REPORT_ERROR(errcode) \
            {  \
	      create_rerror(errcode); \
	      goto done; \
	    } 


/*
  File operations for each file are defined here.
  These functions are shared across many files, but we have the option
   of not doing so. This avoids the use of big ugly switch statements
   to determine which file is being targetted.

  A lot of this has been inspired by Latchesar Ionkov's wonderful npfile
    implementation. Many thanks to Lucho for that.
*/

NpDtfileops defaultfileops = {
	.read = NULL,
	.write = NULL,
	.open = NULL
};

NpDtfileops clonefileops = {
	.read = NULL,
	.write = NULL,
	.open = clone_open
};

NpDtfileops ctlfileops = {
	.read = ctl_read,
	.write = ctl_write,
	.open = NULL
};

NpDtfileops datafileops = {
	.read = data_read,
	.write = data_write,
	.open = NULL
};

NpDtfileops listenfileops = {
	.read = listen_read,
	.write = NULL,
	.open = NULL
};


static Dirtab
statictab[]={
  "root",  Qroot,  Qtdir, Nobody, &defaultfileops,
  "clone", Qclone, Qtfile, Qroot, &clonefileops

};

static Dirtab
clonetab[]={
  "root",  Qtopdir,  Qtdir, Nobody, &defaultfileops,
  "ctl", Qctl, Qtfile, Qtopdir, &ctlfileops,
  "data", Qdata, Qtfile, Qtopdir, &datafileops,
  "listen", Qlisten, Qtfile, Qtopdir, &listenfileops
  
};



/* 
   Create a qid value for based on a connection structure.
*/
void ConnPtr2Qid(Conn *connptr, Npqid *qid, Dirtab *dt)
{
  u64 tmp1, tmp2, tmp3, tmp4;

  qid->type = dt->qidtype;
  qid->version = 0;
  qid->path = dt->qidpath;
  qid->path |= connptr->index << IndexShift;

}

/* is the given file in the dynamic (cloned) part of the filesystem   */
int IS_IN_DYNAMIC(Fid *f)
{
  int dirnum;

  dirnum = CONNINDEX((u64)f->qid.path) & IndexMask;
  return (dirnum != 0);
}

int IS_IN_STATIC(Fid *f)
{
  int retval;
  
  retval = IS_IN_DYNAMIC(f);
  return !(retval);
}


static int retrieveFileSpecs(Fid *f, Dirtab **ptabptr, int *ptabsize, Conn **pconnptr)
{
  *pconnptr = NULL;

  if (IS_IN_STATIC(f)) {
    *ptabsize = NELEM(statictab); *ptabptr = statictab;
    return 1;
  }

  if (IS_IN_DYNAMIC(f)) {
    *ptabsize = NELEM(clonetab); *ptabptr = clonetab;
    *pconnptr = getConnPtr(CONNINDEX((u64) f->qid.path));
    return 1;
  }

  return -1;
}

   
/* sets a qid structure according to a given dirtab entry  */
void dt2qid(Dirtab *dtentry, Npqid *qid, Conn *connptr)
{

    qid->type = dtentry->qidtype;
    qid->version = 0; /* just a dummy */
    
    qid->path = dtentry->qidpath;  

    if (connptr != NULL ) 
      qid->path |= (connptr->index << IndexShift);

}


/* 
   sets the fid to correspond to a file in the filesystem directory table  
   NOTE: connptr is either passed to us (in case we are crossing a TransferPoint,
   and otherwise we need to figure it out ourselves
*/
void dt2fid(Dirtab *dtentry, Fid *fid, char *filename, Conn *connptr)
{
  Dirtab *tabptr;
  int tabsize;

  if (connptr == NULL)
    if (retrieveFileSpecs(fid, &tabptr, &tabsize, &connptr) < 0) 
      return;

  dt2qid(dtentry, &(fid->qid), connptr);
  fid->filename = filename;
  fid->dt = dtentry;
}	  


void init_netfs()
{
  init_conn();
}

Npfcall*
return_open(Fid *f)
{
        Npqid qid;

        if (np_haserror()) 
	  return NULL;

	fid2qid(f, &qid);
	return np_create_ropen(&qid, 0);
}

/* opening clone file creates a new connection */
Npfcall*
clone_open(Fid *f, u8 mode)
{
       Conn *newconn;
       TransferPoint *t;
       Npqid qid;

       newconn = findfreeConn();
       if (newconn == NULL) 
	 REPORT_ERROR(ENOSPC);

       /* point to the control file in the role directory */
       ConnPtr2Qid(newconn, &qid, clonetab); 
  
       /* make an entry in the transfer point - cross over in the directory
	  for the new connection
       */
       t = TPCreateNConfigTransferPoint(Qroot, qid.path, clonetab, NELEM(clonetab), newconn, newconn->dirname);
  
       /* now the fid has to point to ctl file in node dir  */
       dt2fid(&(clonetab[1]), f, clonetab[1].name, newconn);	  

       f->parenttab = clonetab;
       f->parenttabsize = NELEM(clonetab);
       /*  QIDCPY(f->qid, qid); */

 done:
       return_open(f);
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
ctl_read(Fid *f, u64 offset, u32 count, Npfcall *ret)
{
	int n;
	Dirtab *tabptr;
	int tabsize;
	Conn *connptr;

	if (retrieveFileSpecs(f, &tabptr, &tabsize, &connptr) < 0)
	  REPORT_ERROR(ENOENT);

	if (connptr == NULL) 
	  REPORT_ERROR(ENOENT);
	
	if (offset > 0) {
	  n = 0;
	  goto done;
	}
	
	sprintf(ret->data,"%d", connptr->index);
	n = strlen(ret->data);
 done:
	return_read(ret, n);


}

 Npfcall*
data_read(Fid *f, u64 offset, u32 count, Npfcall *ret)
{
	int n;
	Dirtab *tabptr;
	int tabsize;
	Conn *connptr;

	if (retrieveFileSpecs(f, &tabptr, &tabsize, &connptr) < 0)
	  REPORT_ERROR(ENOENT);

	if (connptr->status != STATUS_CONNECTED)
	  REPORT_ERROR(ENOTCONN);

	if ((n = getDataFromConnection(connptr, ret->data, count)) < 0)
	  REPORT_ERROR(errno);	      

 done:
	return_read(ret, n);
}
	
 Npfcall*
listen_read(Fid *f, u64 offset, u32 count, Npfcall *ret)
{
	int n;
	Dirtab *tabptr;
	int tabsize;
	Conn *connptr, *newconnptr;
	TransferPoint *t;
	Npqid qid;	

	if (retrieveFileSpecs(f, &tabptr, &tabsize, &connptr) < 0)
	  REPORT_ERROR(ENOENT);

	if (offset > 0) {
	  n = 0;
	  goto done;
	}

	if (connptr->status != STATUS_ASSIGNED)
	  REPORT_ERROR(EBADF);
	if ((n = listenOnConnection(connptr)) < 0)
	  REPORT_ERROR(errno);	    
	newconnptr = getConnPtr(n);
	ConnPtr2Qid(newconnptr, &qid, clonetab); 
	t = TPCreateNConfigTransferPoint(Qroot, qid.path, clonetab, NELEM(clonetab), newconnptr, newconnptr->dirname);
	sprintf(ret->data,"%d", newconnptr->index);
	n = strlen(ret->data);

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
ctl_write(Fid *f, u64 offset, u32 count, u8 *data, Npreq *req)
{
	int n, thistime;
	Dirtab *tabptr;
	int tabsize;
	Conn *connptr;
	char stringopt[KNAMELEN];
	char ipaddress[KNAMELEN];
	char port[KNAMELEN];
	Npqid qid;

        n = count;

	if (retrieveFileSpecs(f, &tabptr, &tabsize, &connptr) < 0)
	  REPORT_ERROR(ENOENT);

	thistime = mygetstringopt(stringopt, (char **)&data, count);
	if (strcmp(stringopt, "connect") == 0) {
	  
	  /* first argument =  ip address */
	  thistime = mygetstringopt(ipaddress, (char **)&data, (count -= thistime));
	  if (thistime < 7) 
	    REPORT_ERROR(EINVAL);
	  
	  /*  second argument = port */
	  thistime = mygetstringopt(port, (char **)&data, (count -= thistime));
	  if (count == 0)
	    REPORT_ERROR(EINVAL);
	  
	  if (createConnection(connptr, ipaddress, port) < 0)
	    REPORT_ERROR(errno);
	  
	} else 
	  if (strcmp(stringopt, "disconnect") == 0) {
	    n = count;
	    if (closeConnection(connptr) < 0)
	      REPORT_ERROR(errno);
	  } else
	    if (strcmp(stringopt, "release") == 0) {
	      if (connptr->status != STATUS_DISCONNECTED)
		REPORT_ERROR(EADDRINUSE);
	      n = count;
	      ConnPtr2Qid(connptr, &qid, clonetab); 
	      if (releaseConnection(connptr) < 0)
		REPORT_ERROR(errno);
	      if (TPReleaseTransferPoint(Qroot, qid.path) < 0)
		fprintf(stderr, "Warning: no transfer point found for released connectin.\n");
	    }
	    else 
	      if (strcmp(stringopt, "port") == 0) {
		n = count;
		/* first argument = port number to associate with */
		thistime = mygetstringopt(port, (char **)&data, (count -= thistime));
		/* can't set the port number while in use */
		if (connptr->status != STATUS_DISCONNECTED)
		  REPORT_ERROR(EADDRINUSE);
		if (assignPort(connptr, port) < 0)
		  REPORT_ERROR(errno);
	      } else /* any other command is illegal  */
		REPORT_ERROR(EINVAL);
 done:
	return_write(n);
}
	
Npfcall*
data_write(Fid *f, u64 offset, u32 count, u8 *data, Npreq *req)
{
	int n;
	Dirtab *tabptr;
	int tabsize;
	Conn *connptr;

	if (retrieveFileSpecs(f, &tabptr, &tabsize, &connptr) < 0)
	  REPORT_ERROR(ENOENT);
    
	if (connptr->status != STATUS_CONNECTED)
	  REPORT_ERROR(ENOTCONN);
	if ((n = sendDataToConnection(connptr, data, count)) < 0)
	  REPORT_ERROR(errno);	      

done:
	return_write(n);
}
	
	
void
usage()
{
  fprintf(stderr, "Usage: netfs [-p port] [-m path to mount point] [-l logfile]\n");
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

	npfile_init_dirtab(srv, statictab, NELEM(statictab));

	init_netfs();

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

