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

  CC		= gcc
  CFLAGS	= -DDEBUG \
	-O2 -Wall -Wstrict-prototypes -I.. -fomit-frame-pointer 
  CONFIG=../config/udf.h
  UDFINC=../linux/udf_fs.h
  LD		= ld
  LD_RFLAGS	=
  TOPDIR 	= .

EXE=dump taglist

all: $(EXE)

dump: 	dump.c
	$(CC) dump.c -o $@

taglist:	taglist.c
	$(CC) taglist.c -o $@

clean:
	/bin/rm -f *.o $(EXE)
