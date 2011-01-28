#ifndef _NETFS_H
#define _NETFS_H 1

/* Qid path values for files within the static sections of the file tree */
enum {
  Qroot,
  Qclone,
  Nobody = 0xff
};

/* Qid path values for files within the cloned sections - Note this
   can share values with Qid path values in the static section because
   the entire qid.path value would be different in certain higher bits
   */
enum {
  Qtopdir,
  Qctl,
  Qdata,
  Qlisten
};


#define Qidbits 0xff

/* number of bits to shift to get to the index number */
#define IndexShift 8  
#define IndexMask 0xff

/* 
   what is the connection index of this path?
   NOTE: index '0' means the file exists in the static part
  */
#define CONNINDEX(qidpath) ((qidpath >> IndexShift) & IndexMask)

#define ISDIRfid(fid) ((fid).qid.type & Qtdir)

 Npfcall*
 clone_open(Fid *f, u8 mode);

 Npfcall*
 ctl_read(Fid *f, u64 offset, u32 count, Npfcall *ret);

 Npfcall*
 data_read(Fid *f, u64 offset, u32 count, Npfcall *ret);

 Npfcall*
 listen_read(Fid *f, u64 offset, u32 count, Npfcall *ret);

Npfcall*
ctl_write(Fid *fid, u64 offset, u32 count, u8 *data, Npreq *req);

Npfcall*
data_write(Fid *fid, u64 offset, u32 count, u8 *data, Npreq *req);



#endif /*  _NETFS_H */
