/*
  This file defines a transfer point where a cloned directory can be added to a dirtab based file system
*/

typedef struct TransferPoint TransferPoint;

struct TransferPoint {
  /* Qid in the original dirtab from where we are walking */
  u64 startQidPath;  

  /* name of directory that is being linked - this is outside the starting dirtab */
  char *destptr;

  /* directory table of location we are walking to */
  Dirtab *desttable;

  int desttablesize;
  
  /* 
     Qid of the top level directory being linked into - this will be copied into
     the fid and returned after the walk is done 
  */
  u64 destQidPath;

  /*  
      a handle to the structure corresponding to the destination directory - the 
      actual type of this varies according to the clone file, hence the void* type.
      This type will be deciphered based on the destination qid path.
  */
  void *handle;

  TransferPoint *next;
};

TransferPoint *TPCreateTransferPoint();
TransferPoint *TPCreateNConfigTransferPoint(u64 qstart, u64 qend, Dirtab *transferdirtab, int transferdirtabsize, void *handle, char *destination);
TransferPoint *TPgetInitalTransferPoint();
void initTP();
int TPReleaseTransferPoint(u64 parentpath, u64 childpath);
