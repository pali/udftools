#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "../nsrHdrs/nsr.h"
#include "chkudf.h"
#include "protos.h"

int main(int argc, char **argv)
{
  char   *devname;
  struct stat fileinfo;

/*
 * Initialize cache management structures
 */
  initialize();

/*
 * Find the name of the file or device we're talking to
 */
  if (argc > 1) {
    devname = argv[1];
  } else {
    devname = getenv("DEVICE");
  }

  if (devname) {
    device = open(devname, O_RDONLY);

    if (device > 0) {
      printf("--Determining device/media parameters.\n");
      SetSectorSize();
      SetLastSector();
      if (LastSector == -1) {
        fstat(device, &fileinfo);
        LastSector = (fileinfo.st_size >> sdivshift) - 1;
      }
      printf("  Last Sector = %d (0x%x) and is%s accurate\n", LastSector, 
             LastSector, LastSectorAccurate ? "" : " not");
      if (!LastSectorAccurate) {
        SetLastSectorAccurate();
      }
      if (isType5) {
        SetFirstSector();
      }
      Check_UDF();
      cleanup();
      close(device);
    } else {
      printf("**Can't open %s (error %d)\n", devname, errno);
    }
  } else {
    printf("**You must either specify the device or set the DEVICE environment variable.\n");
  }
  return 0;
}
