#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

int CheckRegid(struct udfEntityId *reg, char *ID)
{
  int error = 0;
  UINT8 cbuf[23];

  strncpy(cbuf, ID, 23);

  if (!reg) {
    error = 1;
  }
  if (strncmp(reg->aID, cbuf, 23)) {
    error = 1;
  }
  if (reg->uOSClass > 6) {
    error = 1;
  }
  if ((U_endian16(reg->uUDFRevision) < 0x100) || (U_endian16(reg->uUDFRevision) > 0x200)) {
    error = 1;
  }
  return error;
}

/********************************************************************/
/* Display only the first part of a regid                           */
/********************************************************************/
void DisplayRegIDID( struct regid *RegIDp)
{
    /* assumes character is positioned */
    /* Make a null-terminated version of the Identifier field. */
    char id[24];
    memcpy(id,RegIDp->aRegisteredID,23);
    id[23] = '\000';

    if ( RegIDp->uFlags & DIRTYREGID )
        printf("(Dirty **NON-UDF**)     ");
    if ( RegIDp->uFlags & PROTECTREGID )
        printf("(Protected **NON-UDF**) ");

    printf("'%.23s'",id);
}

void printOSInfo( UINT8 osClass, UINT8 osIdentifier )
{
    printf(" OS: %u,%u",
           osClass,osIdentifier);
    switch (osClass) {
    case OSCLASS_UNDEF: printf(" (Undefined)"); break;
    case OSCLASS_DOS:   printf(" (DOS)");       break;
    case OSCLASS_OS2:   printf(" (OS/2)");      break;
    case OSCLASS_MAC:   printf(" (Macintosh)"); break;
    case OSCLASS_UNIX:  printf(" (UNIX)");      break;
    case OSCLASS_WIN95: printf(" (Windows 9x)"); break;
    case OSCLASS_WINNT: printf(" (Windows NT)"); break;
    default:            printf(" (Illegal) ** NON-UDF 1.50 **");
    }

    if (osClass == OSCLASS_UNIX) {
        switch (osIdentifier) {
        case OSID_GENERIC:     printf(" (Undefined)"); break;
        case OSID_IBM_AIX:     printf(" AIX");       break;
        case OSID_SUN_SOLARIS: printf(" Solaris");      break;
        case OSID_HPUX:        printf(" HPUX"); break;
        case OSID_SGI_IRIX:    printf(" SGI_Irix");      break;
	case OSID_LINUX:       printf(" Linux"); break;
	case OSID_MKLINUX:     printf(" MkLinux"); break;
	case OSID_FREEBSD:     printf(" FreeBSD"); break;
        default:               printf(" (Unknown) ** NON-UDF1.50 **");
        }
    } else if (osClass == OSCLASS_WIN95) {
        switch (osIdentifier) {
          case 0:              printf(" (95)"); break;
          case 1:              printf(" (98)"); break;
          default:             printf(" (Unknown)"); break;
        }
    } else {
        if (osIdentifier != 0)
            printf("\n  ** NON-UDF **");
    }
}

/**************************/
/* Display a UDF EntityID */
/**************************/
void DisplayUdfID(struct udfEntityId * ueip)
{
  int i;
  /* Display the Identifier and flags field first */
  DisplayRegIDID((struct regid *)ueip);

  /* Then display the suffix */
  printf(", UDF Ver.: %02x.%02x",
         (U_endian16(ueip->uUDFRevision) & 0xff00) >> 8, U_endian16(ueip->uUDFRevision) & 0xff);
  printOSInfo(ueip->uOSClass,ueip->uOSIdentifier);
  if (*(ueip->aReserved) != 0) {
    printf(", Reserved: ");
    for (i = 0; i < 4; i++) {
      printf("%02x ", ueip->aReserved[i]);
    }
  }
  printf("\n");
}

/********************************/
/* Display an implementation ID */
/********************************/

void DisplayImplID(struct implEntityId * ieip)
{
  int i;
  /* Display the Identifier and flags field first */
  DisplayRegIDID((struct regid *)ieip);

  /* Then display the suffix */
  printOSInfo(ieip->uOSClass,ieip->uOSIdentifier);
  if (*(ieip->uImplUse) != 0) {
    printf(", Impl. use: ");
    for (i = 0; i < 6; i++) {
      printf("%02x ", ieip->uImplUse[i]);
    }
  }
  printf("\n");
}
