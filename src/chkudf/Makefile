OBJS=chkudf.o read_udf.o errors.o utils.o verifyVRS.o verifyAVDP.o \
	globals.o checkTag.o verifyVDS.o verifyVD.o verifyRegid.o \
        setLastSector.o build_scsi.o utils_read.o init.o \
        cleanup.o volspace.o getVAT.o getMap.o display_dirs.o verifyICB.o \
        readSpMap.o filespace.o icbspace.o linkcount.o setSectorSize.o \
        setFirstSector.o do_scsi.o verifyLVID.o

chkudf: $(OBJS)
	gcc -o chkudf -ggdb $(OBJS)

.c.o:
	gcc -I../../../linux/include -Wall -ggdb -c $*.c

clean:
	rm chkudf *.o

chkudf.o: chkudf.c chkudf.h

read_udf.o: read_udf.c chkudf.h

