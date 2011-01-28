/*
  Utilities used in the file system implementations.
Note: some functions have been lifted from/ inspired by inferno source code. 
*/


#include <string.h>
#include "npfs.h"
#include "casafs.h"
#include "myutils.h"

void QIDCPY(Npqid fromqid, Npqid *toqid) 
{ 
  toqid->type = (fromqid).type; 
  toqid->version = (fromqid).version; 
  (toqid)->path = (fromqid).path;
}

void
fid2qid(Fid *fid, Npqid *qid)
{
  QIDCPY(fid->qid, qid);
}

void initNpqid(Npqid *q)
{
  q->type = 0;
  q->version = 0;
  q->path = 0;
}

/* creates an instance of filesystem fid */
Fid* 
npfs_fidalloc() {
	Fid *f;

	f = malloc(sizeof(*f));
	initNpqid(&(f->qid)); // initialization value
	f->omode = -1;
	f->offset = 0;
	return f;
}

void
create_rerror(int ecode)
{
	char buf[256];
	char *ename;

	ename = strerror_r(ecode, buf, sizeof(buf));
	np_werror(ename, ecode);
}

int ISDIRqid(Npqid q) 
{
  int retval;
  retval = q.type & Qtdir;
  return retval;
}

char hex2char(int h)
{
  if (!((h<16) && (h>=0))) {
    printf("Invalid arg: %d for hex2int\n",h);
    exit(-1);
  }

  if (h<10)
    return ('0'+h);
  else
    return ('A'+(h - 10));
}

int char2hex(char c)
{
  if (!(( (c>='0') && (c <= '9')) || ((c >= 'A') && (c <= 'F')))) {
    printf("Invalid arg %c for char2hex\n",c);
    exit(-1);
  }

  if ( (c>='0') && (c <= '9'))
    return (c - '0');

  if ((c >= 'A') && (c <= 'F'))
    return (10 + c - 'A');

  return -1;
}

/* convert a string of given length to integer  */
int myatoi(char *cdata, int ccount)
{
  int i;
  
  i = 0;
  while ((isdigit(*cdata)) && ccount) {
    i = (i * 10) + (*cdata - '0');
    cdata++;
    ccount--;
  }
  if (ccount)
    return -1;
  return i;
    
}

int
ncmdfield(char *p, int n)
{
	int white, nwhite;
	char *ep;
	int nf;

	if(p == NULL)
		return 1;

	nf = 0;
	ep = p+n;
	white = 1;	/* first text will start field */
	while(p < ep){
		nwhite = (strchr(" \t\r\n", *p++ & 0xFF) != 0);	/* UTF is irrelevant */
		if(white && !nwhite)	/* beginning of field */
			nf++;
		white = nwhite;
	}
	return nf+1;	/* +1 for nil */
}

/*
  get the next argument from the string passed in.
  The size of the string in n chars.
  Arguments = anything delineated by whitespaces
*/
int mygetstringopt(char *to, char **from, int n)
{
  int count;

  count = 0;

  while ((strchr(" \t\r\n", **from & 0xFF)) && (n > 0)) { 
    *from = *from + 1;
    n--;
    count++;
  }

  while ((!strchr(" \t\r\n", **from & 0xFF)) && (n > 0)) {
    *to++ = **from;
    count++;
    *from = *from + 1;
    n--;
  }
  *to = '\0';
  return count;
}

int mystrcat(char *dest, char *src)
{
  strcpy(dest, src);
  dest += strlen(src);
  return (strlen(src));
}
