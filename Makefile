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

CHKUDFDIR	= src/chkudf
CHKUDFDEP	= $(CHKUDFDIR)/chkudf

EXE=dump dumpfe taglist cdinfo mkudf chkudf

all: $(EXE)

dump: 	src/dump.c
	$(CC) $< $(CFLAGS) -o $@

dumpfe:	src/dumpfe.c
	$(CC) $< $(CFLAGS) -o $@

cdinfo: src/cdinfo.c
	$(CC) $< $(CFLAGS) -o $@

taglist: src/taglist.c
	$(CC) -I.. $< $(CFLAGS) -o $@

mkudf:	src/mkudf.c $(LIBS)
	@echo "* ------------------------------ *"
	@echo "* Making Ben Fennema's mkudf ... *"
	@echo "* ------------------------------ *"
	$(CC) -I.. $< $(CFLAGS) $(LIBS) -o $@

chkudf: $(CHKUDFDEP)
	cp $(CHKUDFDEP) .

$(CHKUDFDEP):
	@echo "* ---------------------------- *"
	@echo "* Making Rob Simm's chkudf ... *"
	@echo "* ---------------------------- *"
	$(MAKE) -C $(CHKUDFDIR)

clean:
	@-/bin/rm -f *.o $(EXE)
	@-make -C $(CHKUDFDIR) clean
	@echo "cleaned."
