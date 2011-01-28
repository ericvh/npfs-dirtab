#ifndef _CONSOLEFS_H
#define _CONSOLEFS_H


/* Qid path values for files within the static sections of the file tree */
enum {
  Itopdir,
  Icons,
  Itime,
  Imsec,
  Nobody = 0xff
};

#define ISDIRfid(fid) ((fid).qid.type & Qtdir)

/* prototypes for the read functions  */
Npfcall*
cons_read(Fid *f, u64 offset, u32 count, Npfcall *ret);

Npfcall*
time_read(Fid *f, u64 offset, u32 count, Npfcall *ret);

Npfcall*
msec_read(Fid *f, u64 offset, u32 count, Npfcall *ret);

Npfcall*
cons_write(Fid *fid, u64 offset, u32 count, u8 *data, Npreq *req);



#endif
