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

include ../config.mk

UDF_INC = -I../include -I../src -I$(LINUX)/include

CFLAGS	= $(UDF_INC)\
	-O2 -Wall -Wstrict-prototypes -I..  -g
LIBS		= ../lib/libudf.a

CHKUDFDIR	= src/chkudf
CHKUDFDEP	= $(CHKUDFDIR)/chkudf

EXE=dumpsect dumpfe taglist cdinfo mkudf chkudf dumpea checkdisk bmap

all: ../.prereq.ok $(EXE)

install: $(EXE)
	@echo "TDEST= $(TOOLS_DEST)"
	$(INSTALL) $^ $(TOOLS_DEST)

dumpsect: 	src/dumpsect.c
	$(CC) $< $(CFLAGS) -o $@

dumpfe:	src/dumpfe.c
	$(CC) $< $(CFLAGS) -o $@

dumpea: src/dumpea.c
	$(CC) $< $(CFLAGS) -o $@

cdinfo: src/cdinfo.c
	$(CC) $< $(CFLAGS) -o $@

taglist: src/taglist.c
	$(CC) -I.. $< $(CFLAGS) -o $@

checkdisk: src/checkdisk.c
	$(CC) $< $(CFLAGS) -o $@

bmap: src/bmap.c
	$(CC) $< $(CFLAGS) -o $@

#
# to use the loop device:
# mount -t udf <filename> /mnt -o loop
#
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
	@-/bin/rm -f *.o $(EXE) *~ *.bak
	@-make -C $(CHKUDFDIR) clean
	@echo "tools cleaned."
