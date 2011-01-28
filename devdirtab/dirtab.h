#ifndef _DIRTAB_H
#define _DIRTAB_H

#define ISDIRfid(fid) ((fid).qid.type & Qtdir)	

#define Qidbits 0xff

void
npfile_init_dirtab(Npsrv *srv, Dirtab *dt, int tabsize);

#endif
