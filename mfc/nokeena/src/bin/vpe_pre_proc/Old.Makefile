topdir = ..
srcdir = $(topdir)/src
includedir = -I$(topdir)/include -I/usr/include/glib-2.0/ -I/usr/lib64/glib-2.0/include -I/home/sunil/sf/exp1.0.1/nokeena/src/include
etagsdir = $(topdir)/include/*.h /usr/include/glib-2.0/*.h /usr/lib64/glib-2.0/include/*.h /home/sunil/sf/exp1.0.1/nokeena/src/include/*.h
outputdir = $(topdir)/bin
output = nkn_flv_pre_proc

SRCS = \
	$(srcdir)/flv_olap_chunking.c\
	$(srcdir)/flv.c\
	$(srcdir)/types.c\
	$(srcdir)/nkn_bitstream.c\
	$(srcdir)/nkn_file_io.c\
	$(srcdir)/backend_io_plugin.c\
	$(srcdir)/unit_test.c\
	$(srcdir)/codec_plugin.c\

CFLAGS = -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -g -Wall -Werror
LDFLAGS = -L/usr/lib64 -lm -lglib-2.0 
CC = gcc
driver: $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(includedir) $(LDFLAGS) -o $(outputdir)/$(output) 

etags:
	etags $(srcdir)/*.c $(etagsdir)
clean:
	rm -f *.o $(outputdir)/nkn_flv_pre_proc
