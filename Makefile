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

UDF_INC = -I../include -I../linux-$(UTS_SHORT) -I$(LINUX)/include

CFLAGS	= $(UDF_INC)\
	-O2 -Wall -Wstrict-prototypes -I..  -g
LIBS		= ../lib/libudf.a

CHKUDFDIR	= src/chkudf
CHKUDFDEP	= $(CHKUDFDIR)/chkudf

EXE=dumpsect pdumpsect dumpfe taglist cdinfo mkudf chkudf dumpea checkdisk bmap cdrwtool pktsetup

all: ../.prereq.ok $(EXE)

install: $(EXE)
	@echo "TDEST= $(TOOLS_DEST)"
	$(INSTALL) $^ $(TOOLS_DEST)

dumpsect: 	src/dumpsect.c
	$(CC) $< $(CFLAGS) -o $@

pdumpsect:	src/pdumpsect.c
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

mkudf:	src/mkudf_main.c src/mkudf.c $(LIBS)
	@echo "* ------------------------------ *"
	@echo "* Making Ben Fennema's mkudf ... *"
	@echo "* ------------------------------ *"
	$(CC) -I.. src/mkudf_main.c src/mkudf.c $(CFLAGS) $(LIBS) -o $@

cdrwtool:	src/mkudf.c src/cdrwtool.c $(LIBS)
	@echo "* -------------------------------- *"
	@echo "* Making Jens Axboe's cdrwtool ... *"
	@echo "* -------------------------------- *"
	$(CC) -I.. src/mkudf.c src/cdrwtool.c $(CFLAGS) $(LIBS) -o $@

pktsetup:	src/pktsetup.c
	@echo "* -------------------------------------------------------- *"
	@echo "* Making Jens Axboe's pktsetup                         ... *"
	@echo "* major=42, minor=0-4                                  ... *"
	@echo "* ftp.kernel.org/pub/linux/kernel/people/axboe/packet/ ... *"
	@echo "* -------------------------------------------------------- *"
	$(CC) $< $(CFLAGS) -o $@

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
