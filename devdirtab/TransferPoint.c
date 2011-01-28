#include <stdlib.h>
#include <stdio.h>
#include "npfs.h"
#include "casafs.h"
#include "TransferPoint.h"

/*
  This file defines a transfer point where a cloned directory can be added to a dirtab based file system
*/

/*
  How do we maintain all the transer points. At some point of time, it might be efficient to index them based on the origin of transfer. But for now, just create a link list.
*/
TransferPoint *transferList;
pthread_mutex_t	globallock;

void init_TP()
{
  transferList = NULL;
  pthread_mutex_init(&globallock, NULL);
}

/*
  Create a new transfer point (used when adding a new clone dir) and add at the start
  
*/

TransferPoint *TPCreateTransferPoint()
{
  TransferPoint *new;
  
  new = (TransferPoint *) malloc(sizeof(TransferPoint));
  new->next = transferList;
  transferList = new;
  return new;
}

TransferPoint *TPCreateNConfigTransferPoint(u64 qstart, u64 qend, Dirtab *transferdirtab, int transferdirtabsize, void *handle, char *destination)
{
  TransferPoint *new;
  
  pthread_mutex_lock(&globallock);
  new = (TransferPoint *) malloc(sizeof(TransferPoint));
  new->next = transferList;
  transferList = new;

  new->startQidPath = qstart;
  new->destQidPath = qend;
  new->desttable = transferdirtab;
  new->desttablesize = transferdirtabsize;
  new->handle = handle;
  new->destptr = destination;
  pthread_mutex_unlock(&globallock);
  return new;
}

int TPReleaseTransferPoint(u64 parentpath, u64 childpath) 
{
  TransferPoint *tp, *tpnext;

  pthread_mutex_lock(&globallock);
  tp = TPgetInitalTransferPoint();
  if (tp == NULL) {
    pthread_mutex_unlock(&globallock);
    return -1;
  }

  if ((tp->startQidPath == parentpath) && (tp->destQidPath == childpath)) {
    transferList = tp->next;
    free(tp);
    pthread_mutex_unlock(&globallock);
    return 1;
  }

  tpnext = tp->next;

  while (tpnext) {
  if ((tpnext->startQidPath == parentpath) && (tpnext->destQidPath == childpath)) {
    tp->next = tpnext->next;
    free(tpnext);
    return 1;
  }
  tp = tpnext;
  tpnext = tpnext->next;
  }

  pthread_mutex_unlock(&globallock);
  return -1;
}


char *getTPdestination(TransferPoint *tp)
{
  return tp->destptr;
}
  
TransferPoint *TPgetInitalTransferPoint()
{
  return transferList;
}
