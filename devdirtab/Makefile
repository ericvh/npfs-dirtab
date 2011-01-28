consolefs_OBJECTS =  consolefs.o myutils.o dirtab.o mount-Linux.o TransferPoint.o myconsole.o
consolefs_DEPENDENCIES = 
consolefs_SOURCES =  myconn.c TransferPoint.c mount-Linux.c dirtab.c
consolefs_LDADD = -lpthread -L../libnpfs -lnpfs 
consolefs_LDFLAGS = 

netfs_OBJECTS =  netfs.o myutils.o dirtab.o mount-Linux.o TransferPoint.o myconn.o
netfs_DEPENDENCIES = 
netfs_SOURCES =  myconn.c TransferPoint.c mount-Linux.c dirtab.c netfs.c
netfs_LDADD = -lpthread -L../libnpfs -lnpfs 
netfs_LDFLAGS = 


srcdir = .
top_srcdir = ..

CC = gcc
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir) -I$(srcdir)
CFLAGS = -g -O0
COMPILE = $(CC) $(INCLUDES) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(CFLAGS) $(LDFLAGS) -o $@


%.o: %.c 
	@echo '$(COMPILE) -c $<'; \
	$(COMPILE) -c $<

consolefs: $(consolefs_OBJECTS) $(consolefs_DEPENDENCIES)
	@rm -f consolefs
	$(LINK) $(consolefs_LDFLAGS) $(consolefs_OBJECTS) $(consolefs_LDADD) $(LIBS)

netfs: $(netfs_OBJECTS) $(netfs_DEPENDENCIES)
	@rm -f  netfs
	$(LINK) $(netfs_LDFLAGS) $(netfs_OBJECTS) $(netfs_LDADD) $(LIBS)

ntpclient: ntpclient.o ntpclient.h
	$(LINK) $(ntpclient_OBJECTS)

