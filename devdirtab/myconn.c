/*
 This file implements functions to interact with/control logical nodes within a cluster setup
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
#include <sys/socket.h>   
#include <netinet/in.h>
#include <signal.h>
#include "npfs.h"
#include "casafs.h"
#include "netfs.h"
#include "myconn.h"
#include "TransferPoint.h"

/* something less than 255  */
#define MAXCONN 100
#define MAX_BACKLOG 50

/* to safeguard during global operations */
pthread_mutex_t	globallock;

Conn connArr[MAXCONN];

void init_conn() {
  int i;

  for(i = 1; i < MAXCONN; i++) 
    connArr[i].status = STATUS_FREE;
  pthread_mutex_init(&globallock, NULL);
  
  /* if tcp connections get disconnected in the middle, this will
     catch the signal and just result in error, instead of the 
     program quiting.
  */
  signal(SIGPIPE, SIG_IGN);

}

/* 
   Returns a pointer to a free connection. Surely there are more efficient ways
   of doing this ....
*/
Conn *findfreeConn() 
{
  int index;

  pthread_mutex_lock(&globallock);
  /* start looking from 1 as indices should start from there  */
  for(index = 1; index < MAXCONN; index++)
    if (connArr[index].status == STATUS_FREE) {
      connArr[index].index = index;
      connArr[index].status = STATUS_DISCONNECTED;
      sprintf(connArr[index].dirname,"%d", connArr[index].index);
      pthread_mutex_init(&connArr[index].lock, NULL);
      pthread_mutex_unlock(&globallock);
      return &(connArr[index]);
    }
  pthread_mutex_unlock(&globallock);	
  return NULL;
}

/* to map index back to the connection structure */
Conn *getConnPtr(int connindex)
{
  if ((connindex < MAXCONN) || (connArr[connindex].status == STATUS_FREE))
    return &(connArr[connindex]);
  return NULL;
}

/* connect to the given ip addres and port  */
int createConnection(Conn *connptr, char *ipaddress, char *port)
{
  int portnum;
  int my_socket;
  struct sockaddr_in other_addr;
  int sin_size;
  int retval;

  pthread_mutex_lock(&connptr->lock);
  if (connptr->status != STATUS_DISCONNECTED) {
    errno = EINVAL;
    return -1;
  }

  my_socket=socket(AF_INET,SOCK_STREAM,0);
  if(my_socket<0)
    return -1;

  other_addr.sin_family= AF_INET;
  portnum = myatoi(port, strlen(port));
  if (portnum < 0) {
    errno = EINVAL;
    return -1;
  }
    
  other_addr.sin_port=htons(portnum);
  other_addr.sin_addr.s_addr=inet_addr(ipaddress); 

  if ((retval = connect(my_socket,(struct sockaddr *) &other_addr, sizeof(struct sockaddr))) < 0) {
  pthread_mutex_unlock(&connptr->lock);
  return retval;
  }

  connptr->fd = my_socket;
  connptr->status = STATUS_CONNECTED;
  pthread_mutex_unlock(&connptr->lock);
  return 1;
}

int getDataFromConnection(Conn *connptr, char *buff, int count)	
{
  int retval;
  int myerrno;

  pthread_mutex_lock(&connptr->lock);
  retval = read(connptr->fd, buff, count);
  myerrno = errno;
  if (retval <= 0) {
    connptr->status = STATUS_DISCONNECTED;
    pthread_mutex_unlock(&connptr->lock);
    errno = myerrno;
    return -1;
  }
  
  pthread_mutex_unlock(&connptr->lock);
  return retval;
}

int sendDataToConnection(Conn *connptr, char *data, int count)
{
  int retval;
  int myerrno;

  pthread_mutex_lock(&connptr->lock);
  retval = write(connptr->fd, data, count);
  myerrno = errno;
  if (retval < 0) {
    pthread_mutex_unlock(&connptr->lock);
    errno = myerrno;
    return -1;
  }
  pthread_mutex_unlock(&connptr->lock);
  return retval;
}

int closeConnection(Conn *connptr)
{
  pthread_mutex_lock(&connptr->lock);
  if (connptr->status != STATUS_CONNECTED) {
    pthread_mutex_unlock(&connptr->lock);
    return -1;
  }
  close(connptr->fd);
  connptr->status = STATUS_DISCONNECTED;
  pthread_mutex_unlock(&connptr->lock);
  return 1;
}
  
int assignPort(Conn *connptr, char *port) 
{
  struct sockaddr_in my_addr;
  int portnum;
  int myerrno;

  pthread_mutex_lock(&connptr->lock);
  portnum = myatoi(port, strlen(port));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port=htons(portnum);
 
  connptr->fd = socket(AF_INET,SOCK_STREAM,0);
  if(connptr->fd < 0)
    {
      myerrno = errno;
      pthread_mutex_unlock(&connptr->lock);
      errno = myerrno;
      return -1;
    }
  if (bind(connptr->fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0)
    {
      myerrno = errno;
      pthread_mutex_unlock(&connptr->lock);
      errno = myerrno;
    }
  connptr->status = STATUS_ASSIGNED;
  pthread_mutex_unlock(&connptr->lock);
  return 1;
  
}

int listenOnConnection(Conn *conn)
{
  Conn *newconn;
  int myerrno;
  int sin_size;

  pthread_mutex_lock(&conn->lock);
  conn->status = STATUS_LISTEN;
  pthread_mutex_unlock(&conn->lock);
  if (listen(conn->fd, MAX_BACKLOG) < 0)
    {
      myerrno = errno;
      pthread_mutex_unlock(&conn->lock);
      errno = myerrno;
      return -1;
    }

  /* new entry for the incoming connection  */
  newconn = findfreeConn(); 
  sin_size = sizeof(struct sockaddr_in);

  pthread_mutex_lock(&newconn->lock);
  newconn->fd = accept(conn->fd, (struct sockaddr *)&newconn->connsockaddr, &sin_size);
  newconn->status = STATUS_CONNECTED;
  pthread_mutex_unlock(&newconn->lock);

  pthread_mutex_lock(&conn->lock);
  conn->status = STATUS_ASSIGNED;
  pthread_mutex_unlock(&conn->lock);
  return newconn->index;
}

int releaseConnection(Conn *connptr) 
{
  int retval;

  pthread_mutex_lock(&connptr->lock);
  if (connptr->status != STATUS_DISCONNECTED) {
      pthread_mutex_unlock(&connptr->lock);
      return -1;
  }
    
  connptr->status = STATUS_FREE;
  retval = connptr->index;
  pthread_mutex_unlock(&connptr->lock);
  return retval;
}
