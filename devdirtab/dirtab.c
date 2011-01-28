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
#include "TransferPoint.h"
#include "dirtab.h"


#define REPORT_ERROR(errcode) \
            {  \
	      create_rerror(errcode); \
	      goto done; \
	    } 


Npsrv *srv;
Dirtab *maintab;
int maintabsize;
   
void dt2qid(Dirtab *dtentry, Npqid *qid, void *connptr);
void dt2fid(Dirtab *dtentry, Fid *fid, char *filename, void *connptr);


static Dirtab *findNextDirChild(int *offset, u64 parentpath, Dirtab *fsdirtab, int tabsize, TransferPoint **tpp)
{
  Dirtab *dt;
  TransferPoint *tp;
  int llindex;
  u8 filepath;

  filepath = (u8) (parentpath & Qidbits);

  /* to begin with assume its not a TransferPoint filename we're looking for */
  *tpp = NULL;

  while (*offset < tabsize) {
   dt = &(fsdirtab[*offset]);
   *offset = *offset + 1;
   if (dt->parentpath == filepath) 
     return dt;
  }

  /*
    file being looked for is not in the Dirtab, so now look in transfer points 
  */

  tp = TPgetInitalTransferPoint();
  llindex = tabsize;

  /*
    get to the offset
  */
  while (llindex < *offset) {
    if (tp == NULL)
      return NULL;
    tp = tp->next;
    llindex++;
  }
  
  /* from now on look for a matching name in the destination */
  while (tp) {
    *offset = *offset + 1;
    if (tp->startQidPath == parentpath) {
      *tpp = tp;
      return tp->desttable;
    }
    tp = tp->next;

  }
    
  /* found nothing */
  return NULL;
}

static Dirtab *findNextDirChild1(int *offset, u8 filepath, Dirtab *fsdirtab, int tabsize, TransferPoint **tpp)
{
  Dirtab *dt;
  TransferPoint *tp;
  int llindex;

  /* to begin with assume its not a TransferPoint filename we're looking for */
  *tpp = NULL;

  while (*offset < tabsize) {
   dt = &(fsdirtab[*offset]);
   *offset = *offset + 1;
   if (dt->parentpath == filepath) 
     return dt;
  }

  /*
    file being looked for is not in the Dirtab, so now look in transfer points 
  */

  tp = TPgetInitalTransferPoint();
  llindex = tabsize;

  /*
    get to the offset
  */
  while (llindex < *offset) {
    if (tp == NULL)
      return NULL;
    tp = tp->next;
    llindex++;
  }
  
  /* from now on look for a matching name in the destination */
  while (tp) {
    *offset = *offset + 1;
    if (tp->startQidPath == filepath) {
      *tpp = tp;
      return tp->desttable;
    }
    tp = tp->next;

  }
    
  /* found nothing */
  return NULL;
}

static int dirtab_walk(Npfid *fid, Npstr *wname, Npqid *wqid)
{
	int n;
	Fid *f;
	Dirtab *dt;
	int found, offset;
	Dirtab *tabptr;
	int tabsize;
	TransferPoint *tp;


	dt = NULL; 
	found = 0; /* have we found a child with this name? */
	offset = 0; /* search for the child from start of the directory table */
	f = fid->aux;
	
	fprintf(stderr, "walk: fid <%d> to %s\n", fid->fid, wname->str);
	
	while (!found) {
	  dt = findNextDirChild(&offset, f->qid.path, f->parenttab, f->parenttabsize, &tp);
	  if (dt == NULL) 
	    break;

	  if (np_strcmp(wname, tp? tp->destptr : dt->name) == 0) {
	    /* fill in the qid that we are going to return 
	       and also change the contents to reflect the new file 
	       it is pointing to.
	    */
	    dt2qid(dt, wqid, tp? tp->handle : NULL);   
	    dt2fid(dt, f, tp? tp->destptr : dt->name, tp? tp->handle : NULL);
	    found = 1;
	    f->dt = dt;
	    if (tp) {
	      f->parenttab = tp->desttable;
	      f->parenttabsize = tp->desttablesize;
	    }
	       
	  }
	}
 done:
	if (!found) {
	  create_rerror(ENOENT);
	  return 0;	
	}  
	return 1;

}


static Npfcall*
dirtab_attach(Npfid *nfid, Npfid *nafid, Npstr *uname, Npstr *aname)
{
  	int err;
	Npfcall* ret;
	Fid *fid;
	Npqid qid;
	char *user;

	user = NULL;
	ret = NULL;

	if (nafid != NULL) {
		np_werror(Enoauth, EIO);
		goto done;
	}

	fid = npfs_fidalloc();
	fprintf(stderr, "attach: assigning fid <%d> to root\n", nfid->fid);

	nfid->aux = fid;

	dt2fid(maintab, fid, maintab[0].name, NULL);  /* entry 0 is the root */
	fid->dt = maintab;
	fid->parenttab = maintab;
	fid->parenttabsize = maintabsize;
	fid2qid(fid, &qid);
	ret = np_create_rattach(&qid);
	np_fid_incref(nfid);
done:
	return ret;

}


static int
dirtab_clone(Npfid *fid, Npfid *newfid)
{
	Fid *f, *nf;
	fprintf(stderr, "clone: fid<%d> as fid<%d>\n", newfid->fid, fid->fid);

	f = fid->aux;
	nf = npfs_fidalloc();
	nf->filename = f->filename;
	memcpy(&nf->qid, &f->qid, sizeof(Npqid));
	newfid->aux = nf;
	nf->dt = f->dt;
	
	nf->parenttab = f->parenttab;
	nf->parenttabsize = f->parenttabsize;
	return 1;	
}

static Npfcall*
dirtab_clunk(Npfid *fid)
{
	Fid *f;
	Npfcall *ret;

	fprintf(stderr, "clunk: fid<%d>\n", fid->fid);
	f = fid->aux;
	ret = np_create_rclunk();

	np_fid_decref(fid);
	return ret;
}

static Npfcall*
dirtab_stat(Npfid *fid)
{
	int err;
	Fid *f;
	Npfcall *ret;
	Npwstat wstat;

	f = fid->aux;
	memset(&wstat, 0, sizeof(wstat));

	fprintf(stderr, "stat : fid<%d>\n", fid->fid);
	/* first fill in the qid */
	memmove(&(wstat.qid), &(f->qid), sizeof(Npqid));
	
	/* now mode */
	wstat.mode = UserExecMask | UserWriteMask | UserReadMask;
	if (ISDIRfid(*f))
	  wstat.mode |= Dmdir;
	wstat.atime = 0;
	wstat.mtime = 0;
	wstat.length = 10;

	wstat.uid = "guestuid";
	wstat.gid = "guestgid";
	wstat.muid = "guestmuid";
	wstat.extension = NULL;

	wstat.name = f->filename;

	ret = np_create_rstat(&wstat, 0);

	return ret;
}

static Npfcall*
dirtab_open(Npfid *fid, u8 mode)
{
	int err;
	Fid *f;
	Npqid qid;

	fprintf(stderr, "open: fid<%d>\n",fid->fid);
	f = fid->aux;
	f->omode = mode;
	if (f->dt->fops->open)
	  return f->dt->fops->open(f, mode);

	fid2qid(f, &qid);
	return np_create_ropen(&qid, 0);
}


static u32
dirtab_read_dir(Fid *f, u8* buf, u64 offset, u32 count, int dotu, Dirtab *fstable, int nelem)
{
	int i, n, plen;
	char *dname, *path;
	Npwstat wstat;
	int readoffset;
	Dirtab *dt;
	int index;
	TransferPoint *tp;

	fprintf(stderr, "   readdir ::: dir = %s\n",f->filename); 
	/*  if offset is zero then rewind the directory */
	if (offset == 0) 
	  f->offset = 0;

	n = 0; /* number of bytes written so far*/

	while (n < count) {
	  memset(&wstat, 0, sizeof(wstat));
	  dt = findNextDirChild(&(f->offset), f->qid.path, fstable, nelem, &tp);	  
	  if (dt == NULL)  // no more subdirectories 
	    break;
	  /* first fill in the qid */
	  dt2qid(dt, &(wstat.qid), tp? tp->handle : NULL);
	  
	  /* 
	     the filename has to be set appropriately depending on whether this is a clone dir or not
	  */
	  wstat.name = tp? tp->destptr : dt->name;


	  /* now mode */
	  wstat.mode = UserExecMask | UserWriteMask | UserReadMask;
	  if (ISDIRqid(wstat.qid))
	    wstat.mode |= Dmdir;
	  wstat.atime = 0;
	  wstat.mtime = 0;
	  wstat.length = 10;
	  
	  wstat.uid = "guestuid";
	  wstat.gid = "guestgid";
	  wstat.muid = "guestmuid";
	  wstat.extension = NULL;
	  
	  i = np_serialize_stat(&wstat, buf + n, count - n - 1, dotu);
	  n += i;  /* update number of bytes we are going to return */
	}
	return n;
}


static Npfcall*
dirtab_read(Npfid *fid, u64 offset, u32 count, Npreq *req)
{
	int n;
	Fid *f;
	Npfcall *ret;
	int readint;
	int retsize;
	char b[KNAMELEN];
	u64 timemsec;

	f = fid->aux;

	ret = np_alloc_rread(count);
	if (ISDIRfid(*f)) {
	  n = dirtab_read_dir(f, ret->data, offset, count, fid->conn->dotu, f->parenttab, f->parenttabsize);	  
	  goto done;
	}

	if (f->dt->fops->read)
	  return f->dt->fops->read(f, offset, count, ret);
	else
	  REPORT_ERROR(EPERM);

	fprintf(stderr, "read: fid<%d>\n", fid->fid);

	  

 done:	
	if (np_haserror()) {
		free(ret);
		ret = NULL;
	} else
		np_set_rread_count(ret, n);

	return ret;
}



static Npfcall*
dirtab_wstat(Npfid *fid, Npstat *stat)
{
	int err;
	Fid *f;
	Npfcall *ret;
	uid_t uid;
	gid_t gid;
	char *npath, *p, *s;
	Npuser *user;
	Npgroup *group;

	ret = NULL;
	f = fid->aux;

	if (stat->mode != (u32)~0) 
		if (stat->mode&Dmdir && !ISDIRfid(*f)) {
			np_werror(Edirchange, EIO);
			goto out;
		}
		ret = np_create_rwstat();
	
out:
	return ret;
}

static Npfcall*
dirtab_write(Npfid *fid, u64 offset, u32 count, u8 *data, Npreq *req)
{
	int n;
	Fid *f;
	int retval;
	char lastchar;

	f = fid->aux;

	fprintf(stderr, "write: fid<%d>\n", fid->fid);
	if (ISDIRfid(*f))
		REPORT_ERROR(EPERM);

	if (f->dt->fops->write)
	  return f->dt->fops->write(f, offset, count, data, req);
	else
	  REPORT_ERROR(EPERM);	  
	
 done:
        if (np_haserror()) 
	  return NULL;
	else
	  return np_create_rwrite(n);

}


void
npfile_init_dirtab(Npsrv *srv, Dirtab *dt, int tabsize)
{
	int c; 
	int port, nwthreads;
	char *s;
	char *logfile;
	int issocket;
	char *opts;
	int fd;
	char *mountpath;


	srv->dotu = 0;
	srv->attach = dirtab_attach;
	srv->clone = dirtab_clone;
	srv->walk = dirtab_walk;
	srv->open = dirtab_open;
	//srv->create = lnfs_create;
	srv->read = dirtab_read;
	srv->write = dirtab_write;
	srv->clunk = dirtab_clunk;
	//srv->remove = lnfs_remove;
	srv->stat = dirtab_stat;
	srv->wstat = dirtab_wstat;
	//srv->flush = lnfs_flush;
	//srv->fiddestroy = lnfs_fiddestroy;
	//srv->debuglevel = debuglevel;

	maintab = dt;
	maintabsize = tabsize;


}


