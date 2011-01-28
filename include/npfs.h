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

#include <sys/types.h>
#include <stdint.h>

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef struct Npstr Npstr;
typedef struct Npqid Npqid;
typedef struct Npstat Npstat;
typedef struct Npwstat Npwstat;
typedef struct Npfcall Npfcall;
typedef struct Npfid Npfid;
typedef struct Npbuf Npbuf;
typedef struct Nptrans Nptrans;
typedef struct Npconn Npconn;
typedef struct Npreq Npreq;
typedef struct Npwthread Npwthread;
typedef struct Npauth Npauth;
typedef struct Npsrv Npsrv;
typedef struct Npuser Npuser;
typedef struct Npgroup Npgroup;
typedef struct Npfile Npfile;
typedef struct Npfilefid Npfilefid;
typedef struct Npfileops Npfileops;
typedef struct Npdirops Npdirops;

/* message types */
enum {
	Tfirst		= 100,
	Tversion	= 100,
	Rversion,
	Tauth		= 102,
	Rauth,
	Tattach		= 104,
	Rattach,
	Terror		= 106,
	Rerror,
	Tflush		= 108,
	Rflush,
	Twalk		= 110,
	Rwalk,
	Topen		= 112,
	Ropen,
	Tcreate		= 114,
	Rcreate,
	Tread		= 116,
	Rread,
	Twrite		= 118,
	Rwrite,
	Tclunk		= 120,
	Rclunk,
	Tremove		= 122,
	Rremove,
	Tstat		= 124,
	Rstat,
	Twstat		= 126,
	Rwstat,
	Rlast
};

/* modes */
enum {
	Oread		= 0x00,
	Owrite		= 0x01,
	Ordwr		= 0x02,
	Oexec		= 0x03,
	Oexcl		= 0x04,
	Otrunc		= 0x10,
	Orexec		= 0x20,
	Orclose		= 0x40,
	Oappend		= 0x80,

	Ouspecial	= 0x100,	/* internal use */
};

/* permissions */
enum {
	Dmdir		= 0x80000000,
	Dmappend	= 0x40000000,
	Dmexcl		= 0x20000000,
	Dmmount		= 0x10000000,
	Dmauth		= 0x08000000,
	Dmtmp		= 0x04000000,
	Dmsymlink	= 0x02000000,
	Dmlink		= 0x01000000,

	/* 9P2000.u extensions */
	Dmdevice	= 0x00800000,
	Dmnamedpipe	= 0x00200000,
	Dmsocket	= 0x00100000,
	Dmsetuid	= 0x00080000,
	Dmsetgid        = 0x00040000,

	/* Masks for u/g/o read/write/exec    */
	OtherExecMask   = 0x00000001,
	OtherWriteMask  = 0x00000002,
	OtherReadMask   = 0x00000004,

	GrpExecMask     = 0x00000008,
	GrpWriteMask    = 0x00000010,
	GrpReadMask     = 0x00000020,
	
	UserExecMask    = 0x00000040,
	UserWriteMask   = 0x00000080,
	UserReadMask    = 0x00000100
		
};

/* qid.types */
enum {
	Qtdir		= 0x80,
	Qtappend	= 0x40,
	Qtexcl		= 0x20,
	Qtmount		= 0x10,
	Qtauth		= 0x08,
	Qttmp		= 0x04,
	Qtsymlink	= 0x02,
	Qtlink		= 0x01,
	Qtfile		= 0x00,
};

#define NOTAG		(u16)(~0)
#define NOFID		(u32)(~0)
#define MAXWELEM	16
#define IOHDRSZ		24
#define FID_HTABLE_SIZE 64

struct Npstr {
	u16		len;
	char*		str;
};

struct Npqid {
	u8		type;
	u32		version;
	u64		path;
};

struct Npstat {
	u16 		size;
	u16 		type;
	u32 		dev;
	Npqid		qid;
	u32 		mode;
	u32 		atime;
	u32 		mtime;
	u64 		length;
	Npstr		name;
	Npstr		uid;
	Npstr		gid;
	Npstr		muid;

	/* 9P2000.u extensions */
	Npstr		extension;
	u32 		n_uid;
	u32 		n_gid;
	u32 		n_muid;
};

/* file metadata (stat) structure used to create Twstat message
   It is similar to Npstat, but the strings don't point to 
   the same memory block and should be freed separately
*/
struct Npwstat {
	u16 		size;
	u16 		type;
	u32 		dev;
	Npqid		qid;
	u32 		mode;
	u32 		atime;
	u32 		mtime;
	u64 		length;
	char*		name;
	char*		uid;
	char*		gid;
	char*		muid;
	char*		extension;	/* 9p2000.u extensions */
	u32 		n_uid;		/* 9p2000.u extensions */
	u32 		n_gid;		/* 9p2000.u extensions */
	u32 		n_muid;		/* 9p2000.u extensions */
};

struct Npfcall {
	u32		size;
	u8		id;
	u16		tag;
	u8*		pkt;

	u32		fid;
	u32		msize;			/* Tversion, Rversion */
	Npstr		version;		/* Tversion, Rversion */
	u32		afid;			/* Tauth, Tattach */
	Npstr		uname;			/* Tauth, Tattach */
	Npstr		aname;			/* Tauth, Tattach */
	Npqid		qid;			/* Rauth, Rattach, Ropen, Rcreate */
	Npstr		ename;			/* Rerror */
	u16		oldtag;			/* Tflush */
	u32		newfid;			/* Twalk */
	u16		nwname;			/* Twalk */
	Npstr		wnames[MAXWELEM];	/* Twalk */
	u16		nwqid;			/* Rwalk */
	Npqid		wqids[MAXWELEM];	/* Rwalk */
	u8		mode;			/* Topen, Tcreate */
	u32		iounit;			/* Ropen, Rcreate */
	Npstr		name;			/* Tcreate */
	u32		perm;			/* Tcreate */
	u64		offset;			/* Tread, Twrite */
	u32		count;			/* Tread, Rread, Twrite, Rwrite */
	u8*		data;			/* Rread, Twrite */
	Npstat		stat;			/* Rstat, Twstat */

	/* 9P2000.u extensions */
	u32		ecode;			/* Rerror */
	Npstr		extension;		/* Tcreate */

	Npfcall*	next;
};

struct Npfid {
	Npconn*		conn;
	u32		fid;
	int		refcount;
	u16		omode;
	u8		type;
	u32		diroffset;
	Npuser*		user;
	void*		aux;

	Npfid*		next;	/* list of fids within a bucket */
};

struct Npbuf {
	pthread_mutex_t	lock;
	int		size;
	int		pos;
	u8*		buf;

	void*		aux;
	void		(*changed)(void *);
	void		(*error)(void *, int);
};

struct Nptrans {
	pthread_mutex_t	lock;
	int		connected;
	Npbuf*		txbuf;
	Npbuf*		rxbuf;
	void*		aux;

	void		(*settxbuf)(Nptrans *);
	void		(*setrxbuf)(Nptrans *);
	void		(*error)(Nptrans *);
	void		(*destroy)(Nptrans *);
};

struct Npconn {
	pthread_mutex_t	lock;
	u32		msize;
	int		dotu;
	int		shutdown;
	Npsrv*		srv;
	Nptrans*	trans;
	Npfid**		fidpool;
	Npfcall*	rcall;
	Npbuf		rbuf;
	Npfcall*	wcall;
	Npbuf		wbuf;
	Npfcall*	rcalls;
	Npfcall**	rcallp;
	int		freercnum;
	Npfcall*	freerclist;
	void*		aux;

	Npconn*		next;	/* list of connections within a server */
};

struct Npreq {
	Npconn*		conn;
	u16		tag;
	Npfcall*	tcall;
	Npfcall*	rcall;
	int		cancelled;
	Npreq*		flushreq;
	Npfid*		fid;

	Npreq*		next;	/* list of all outstanding requests */
	Npreq*		prev;	/* used for requests that are worked on */
	Npwthread*	wthread;/* for requests that are worked on */
};

struct Npwthread {
	Npsrv*		srv;
	int		shutdown;
	pthread_t	thread;
	char*		errname;
	u32		errcode;

	Npwthread	*next;
};

struct Npauth {
	Npfcall*	(*auth)(Npfid *afid, Npstr *uname, Npstr *aname);
	Npfcall*	(*attach)(Npfid *afid, Npstr *uname, Npstr *aname);
	Npfcall*	(*read)(Npfid *fid, u64 offset, u32 count);
	Npfcall*	(*write)(Npfid *fid, u64 offset, u32 count, u8 *data);
	Npfcall*	(*clunk)(Npfid *fid);
};	

struct Npsrv {
	u32		msize;
	int		dotu;		/* 9P2000.u support flag */
	void*		srvaux;
	void*		treeaux;
	int		debuglevel;
	Npauth*		auth;

	void		(*start)(Npsrv *);
	void		(*shutdown)(Npsrv *);
	void		(*destroy)(Npsrv *);
	void		(*connopen)(Npconn *);
	void		(*connclose)(Npconn *);
	void		(*fiddestroy)(Npfid *);

	Npfcall*	(*version)(Npconn *conn, u32 msize, Npstr *version);
//	Npfcall*	(*auth)(Npfid *afid, Npstr *uname, Npstr *aname);
	Npfcall*	(*attach)(Npfid *fid, Npfid *afid, Npstr *uname, 
				Npstr *aname);
	Npfcall*	(*flush)(Npreq *req);
	int		(*clone)(Npfid *fid, Npfid *newfid);
	int		(*walk)(Npfid *fid, Npstr *wname, Npqid *wqid);
	Npfcall*	(*open)(Npfid *fid, u8 mode);
	Npfcall*	(*create)(Npfid *fid, Npstr* name, u32 perm, u8 mode, 
				Npstr* extension);
	Npfcall*	(*read)(Npfid *fid, u64 offset, u32 count, Npreq *req);
	Npfcall*	(*write)(Npfid *fid, u64 offset, u32 count, u8 *data, 
				Npreq *req);
	Npfcall*	(*clunk)(Npfid *fid);
	Npfcall*	(*remove)(Npfid *fid);
	Npfcall*	(*stat)(Npfid *fid);
	Npfcall*	(*wstat)(Npfid *fid, Npstat *stat);

	/* implementation specific */
	pthread_mutex_t	lock;
	pthread_cond_t	reqcond;
	int		shuttingdown;
	Npconn*		conns;
	Npwthread*	wthreads;
	Npreq*		reqs_first;
	Npreq*		reqs_last;
	Npreq*		workreqs;
};

struct Npuser {
	char*		uname;
	uid_t		uid;
	Npgroup*	dfltgroup;
	int		ngroups;	
	gid_t*		groups;

	Npuser*		next;
};

struct Npgroup {
	char*		gname;
	gid_t		gid;

	Npgroup*	next;
};

struct Npfile {
	pthread_mutex_t	lock;
	int		refcount;
	Npfile*		parent;
	Npqid		qid;
	u32		mode;
	u32		atime;
	u32		mtime;
	u64		length;
	char*		name;
	Npuser*		uid;
	Npgroup*	gid;
	Npuser*		muid;
	char*		extension;
	int		excl;
	void*		ops;
	void*		aux;

	/* not used -- provided for user's convenience */
	Npfile*		next;
	Npfile*		prev;
	Npfile*		dirfirst;
	Npfile*		dirlast;
};

struct Npfileops {
	int		(*read)(Npfilefid* file, u64 offset, u32 count, 
				u8 *data, Npreq *req);
	int		(*write)(Npfilefid* file, u64 offset, u32 count, 
				u8 *data, Npreq *req);
	int		(*wstat)(Npfile*, Npstat*);
	void		(*destroy)(Npfile*);

	int		(*openfid)(Npfilefid *);
	void		(*closefid)(Npfilefid *);
};

struct Npdirops {
	Npfile*		(*create)(Npfile *dir, char *name, u32 perm, 
				Npuser *uid, Npgroup *gid, char *extension);
	Npfile*		(*first)(Npfile *dir);
	Npfile*		(*next)(Npfile *dir, Npfile *prevchild);
	int		(*wstat)(Npfile*, Npstat*);
	int		(*remove)(Npfile *dir, Npfile *file);
	void		(*destroy)(Npfile*);
};

struct Npfilefid {
	pthread_mutex_t	lock;
	Npfile*		file;
	int		omode;
	void*		aux;
	u64		diroffset;
	Npfile*		dirent;
};

extern char *Eunknownfid;
extern char *Enomem;
extern char *Enoauth;
extern char *Enotimpl;
extern char *Einuse;
extern char *Ebadusefid;
extern char *Enotdir;
extern char *Etoomanywnames;
extern char *Eperm;
extern char *Etoolarge;
extern char *Ebadoffset;
extern char *Edirchange;
extern char *Enotfound;
extern char *Eopen;
extern char *Eexist;
extern char *Enotempty;
extern char *Eunknownuser;

Npsrv *np_srv_create(int nwthread);
void np_srv_remove_conn(Npsrv *, Npconn *);
void np_srv_start(Npsrv *);
void np_srv_shutdown(Npsrv *, int wait);
int np_srv_add_conn(Npsrv *, Npconn *);

Npconn *np_conn_create(Npsrv *, Nptrans *);
void np_conn_reset(Npconn *, u32, int);
void np_conn_shutdown(Npconn *, int);
void np_conn_send_fcall(Npconn *, Npfcall *);
void np_respond(Npreq *, Npfcall *);

Npfid **np_fidpool_create(void);
Npfid *np_fid_find(Npconn *, u32);
Npfid *np_fid_create(Npconn *, u32, void *);
int np_fid_destroy(Npfid *);
void np_fid_incref(Npfid *);
void np_fid_decref(Npfid *);

Nptrans *np_trans_create(void);
void np_trans_destroy(Nptrans *);
void np_trans_set_txbuf(Nptrans *, Npbuf *);
void np_trans_set_rxbuf(Nptrans *, Npbuf *);

int np_deserialize(Npfcall*, u8*, int);
int np_serialize_stat(Npwstat *wstat, u8* buf, int buflen, int dotu);

char *np_strdup(Npstr *str);
int np_strcmp(Npstr *str, char *cs);
int np_strncmp(Npstr *str, char *cs, int len);

void np_set_tag(Npfcall *, u16);
Npfcall *np_create_rversion(u32 msize, char *version);
Npfcall *np_create_rauth(Npqid *aqid);
Npfcall *np_create_rerror(char *ename, int ecode, int dotu);
Npfcall *np_create_rerror1(Npstr *ename, int ecode, int dotu);
Npfcall *np_create_rflush(void);
Npfcall *np_create_rattach(Npqid *qid);
Npfcall *np_create_rwalk(int nwqid, Npqid *wqids);
Npfcall *np_create_ropen(Npqid *qid, u32 iounit);
Npfcall *np_create_rcreate(Npqid *qid, u32 iounit);
Npfcall *np_create_rread(u32 count, u8* data);
Npfcall *np_create_rwrite(u32 count);
Npfcall *np_create_rclunk(void);
Npfcall *np_create_rremove(void);
Npfcall *np_create_rstat(Npwstat *stat, int dotu);
Npfcall *np_create_rwstat(void);
Npfcall * np_alloc_rread(u32);
void np_set_rread_count(Npfcall *, u32);

Npuser* np_uid2user(int uid);
Npuser* np_uname2user(char *uname);
Npgroup* np_gid2group(gid_t gid);
Npgroup* np_gname2group(char *gname);
int np_usergroups(Npuser *u, gid_t **gids);
int np_change_user(Npuser *u);

Nptrans *np_fdtrans_create(int, int);
Npsrv *np_socksrv_create_tcp(int, int*);
Npsrv *np_pipesrv_create(int nwthreads);
int np_pipesrv_mount(Npsrv *srv, char *mntpt, char *user, int mntflags, char *opts);

void np_werror(char *ename, int ecode);
void np_rerror(char **ename, int *ecode);
int np_haserror(void);

Npfile* npfile_alloc(Npfile *parent, char *name, u32 mode, u64 qpath, 
	void *ops, void *aux);
void npfile_incref(Npfile *);
int npfile_decref(Npfile *);
Npfile *npfile_find(Npfile *, char *);
int npfile_checkperm(Npfile *file, Npuser *user, int perm);
void npfile_init_srv(Npsrv *, Npfile *);


/* some useful macros */
#define QIDCPY1(fromqid, toqid) \
    { (toqid).type = (fromqid).type; (toqid).version = (fromqid).version; (toqid).path = (fromqid).path;}


#define ISDIR1(fid) ((fid).qid.type && Qtdir)
