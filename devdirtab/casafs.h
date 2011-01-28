#ifndef _CASAFS_H
#define _CASAFS_H

#define NELEM(x)	(sizeof(x)/sizeof((x)[0]))
#define KNAMELEN 28
#define KPATHLEN 100
#define KCMDLEN 500

typedef struct NpDtfileops NpDtfileops;
typedef struct Dirtab Dirtab;
typedef struct Fid Fid;

struct NpDtfileops {
  Npfcall*	(*read)(Fid *fid, u64 offset, u32 count, Npfcall *ret);
  Npfcall*      (*write)(Fid *fid, u64 offset, u32 count, u8 *data, Npreq *req);
  Npfcall*      (*open)(Fid *fid, u8 mode);
};




struct Dirtab {
  char name[KNAMELEN];
  u_int8_t qidpath;  // a unique id for this file
  u_int8_t qidtype;  // for now just stores whether file is DIR 
  u_int8_t parentpath;
  NpDtfileops *fops;
  //  u_int8_t perm; //needs some work with reard to access controlb
};


struct Fid {
  char *filename;
  Dirtab *dt;
  Npqid	qid;
  int omode;
  int offset;
  Dirtab *parenttab;
  int parenttabsize;
};


/*
struct Fid {
  char *filename;
  Npqid	qid;
  int omode;
  int offset;
    int tabsize;
    Dirtab *parenttab;
    int parenttabsize;
};
*/

#endif /* _CASAFS_H */
