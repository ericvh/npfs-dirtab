#ifndef _MYCONN_H_
#define _MYCONN_H_	1

#include <sys/socket.h>   
#include <netinet/in.h>
#include "casafs.h"

struct Conn {
  char ipaddress[16];  /* ip address this connects to in dotted format */
  char dirname[KNAMELEN]; /* name of dir corresponding to this connection  */
  int fd;  /* system fd used for this connection  */
  u8 index; /* at typical index to identify the connection*/
  u8 status; /* connected / disconnected / blocked / free .....  */
  int port; /* port number associated with this port */
  pthread_mutex_t lock;  /* lock to regulate access to this connection */
  struct sockaddr connsockaddr; /* addr connected to  */
};
typedef struct Conn Conn;

void init_conn();
Conn *findfreeConn();
Conn *getConnPtr(int connindex);
int createConnection(Conn *connptr, char *ipaddress, char *port);
int getDataFromConnection(Conn *connptr, char *buff, int count);
int closeConnection(Conn *connptr);
int sendDataToConnection(Conn *connptr, char *data, int count);
int assignPort(Conn *connptr, char *port);
int listenOnConnection(Conn *connptr);
int releaseConnection(Conn *connptr);

/*
int numServers();  // returns the number of servers out there  
u32 getUID(int); 
int getArch(char *, int); // gets the architecture for given machine 
int getMacaddress(char *, int); // get mac address for given machine 
int getIPaddress(char *, int); // get IP address for given machine 
u8 getStatus(int servernum);
int reservemachine(int machineno);
int releasemachine(int machineno);
int findfreemachine();
int qidpath2index(u8);
int findfreeParamMachine(char *param);
int validMCinIndex(int index); //is there a valid machine@index
*/

/* flags for status */
enum {
  STATUS_FREE,
  STATUS_CONNECTED,
  STATUS_DISCONNECTED,
  STATUS_ASSIGNED,
  STATUS_LISTEN,
  STATUS_BLOCKED 
};

#endif /* _MYCONN_H_  */
