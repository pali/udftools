#
# Makefile for the Linux OSTA-UDF(tm) filesystem support.
#
# 9/24/98 dgb: 	changes made to allow compiling outside
#		of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.SUFFIXES:
.SUFFIXES: .c .o .h .a

# KERNH=-I/usr/src/linux-2.1.129/include
UDF_INC = ../include

CC		= gcc
CFLAGS	= -DDEBUG $(KERNH) -I$(UDF_INC)\
	-O2 -Wall -Wstrict-prototypes -I..  -g
LD		= ld
LD_RFLAGS	=
LIBS		= ../lib/libudf.a

EXE=dump dumpfe taglist mkudf

all: $(EXE)

dump: 	dump.c
	$(CC) dump.c $(CFLAGS) -o $@

dumpfe:	dumpfe.c
	$(CC) dumpfe.c $(CFLAGS) -o $@

taglist:	taglist.c
	$(CC) -I.. taglist.c $(CFLAGS) -o $@

mkudf:	mkudf.c $(LIBS)
	$(CC) -I.. mkudf.c $(CFLAGS) $(LIBS) -o $@

clean:
	/bin/rm -f *.o $(EXE)
