void
fid2qid(Fid *fid, Npqid *qid);


void initNpqid(Npqid *q);


/* creates an instance of filesystem fid */
Fid* 
npfs_fidalloc();



void QIDCPY(Npqid fromqid, Npqid *toqid);

void
create_rerror(int ecode);


int ISDIRqid(Npqid q);


char hex2char(int h);


int char2hex(char c);


int myatoi(char *data, int count);

int
ncmdfield(char *p, int n);

int mygetstringopt(char *to, char **from, int n);

int mystrcat(char *dest, char *src);
