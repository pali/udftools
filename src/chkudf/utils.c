#include "../nsrHdrs/nsr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "chkudf.h"
#include "protos.h"

/* some default values */

#define SPACECHAR "."
#define TITLELEN 78
#define MAXCOLUMNS 8

UINT32 endian32(UINT32 toswap)
{
  return (toswap << 24) | 
         ((toswap << 8) & 0x00ff0000) |
         ((toswap >> 8) & 0x0000ff00) |
         (toswap >> 24);
}

UINT16 endian16(UINT16 toswap)
{
  return (toswap << 8) | (toswap >> 8);
}


#define MASK 0x1021

UINT16 doCRC(UINT8 *buffer, int n)
{
  UINT16 CRC = 0;
  int byte, bit;
  UINT8 bitval, msb;

  if (n > 4080) {
    CRC = 0xffff;
  } else {
    for (byte = 0; byte < n; byte++) {
      for (bit=7; bit>=0; bit--) {
        bitval = (*(buffer + byte) >> bit) & 0x01;
        msb = CRC >> 15;
        CRC <<=  1;
        CRC ^= MASK * (bitval ^ msb);
      }
    }
  }
  return CRC;
}

/*********************************************************************/
/* prints a Dstring field -- no guaranteed trailing null, and length */
/* in last byte.                                                     */
/* Supports OSTA Compressed Unicode, but if no Compression algorithm */
/* supplied, will print out ASCII string.                            */
/*********************************************************************/
void printDstring( char *start, UINT8 fieldLen)
{
    /* First, grab the length of the string */
    UINT8 dstringLen = (UINT8)start[fieldLen - 1];

    /* Then, hand it all off to Dchars */
    printDchars(start, dstringLen);
    printf("\n");
    return;
}

/*********************************************************************/
/* prints a Dchars field -- no guaranteed trailing null, and length  */
/* of Dchars field is max length of string.                          */
/* Supports OSTA Compressed Unicode, but if no Compression algorithm */
/* supplied, will print out ASCII string.                            */
/*********************************************************************/
void printDchars( char *start, UINT8 length)
{
    /* Some (one) local variable(s) */
    UINT16 i;                       /* Index  */
    UINT16 unichar;                 /* Unicode character */

    char tbuff[257];                /* Buffer for non-unicode */

    UINT8 dispLen = length;

    /* First, grab the Compressed Algorithm Number. */
    UINT8 alg = start[0];

    /* Throw out the algorithm byte, if any, and print out compression
       algorithm */
    switch (alg) {
    case 16:
        /* 16-bit Unicode, but ignore the first byte */
        start++;
        dispLen--;
        break;
    case 8:
        /* ASCII, but ignore the first byte */
        start++;
        dispLen--;
        break;
    default:
        /* ASCII, including the first byte */
        break;
    }

    /* Print out the characters. */
    if ( alg == 16 ) {
        printf("\"");
        for (i=0;i<dispLen;i++,i++) {
            unichar = *(start + i) << 8;
            unichar |= (UINT8)*(start + i + 1);
            if ((unichar > 31) && (unichar < 127)) {
              printf("%c", (UINT8)unichar);
            } else {
              printf("[%4x]", unichar);
            }
        }
        printf("\"");
    } else {
        /* Now make a copy of all the bytes, to make sure we have a null
           termination */
        strncpy( tbuff, start, dispLen); /* copy over just enough characters */
        tbuff[dispLen]=0;           /* null terminate the string */
        printf("'%s'", tbuff);    /* print it out */
    }
    return;
}

void printExtentAD(struct extent_ad extent)
{
  printf("%u [0x%08x] @ %u [0x%08x]\n",
	 U_endian32(extent.Length), U_endian32(extent.Length),
	 U_endian32(extent.Location), U_endian32(extent.Location));
}

void printCharSpec(struct charspec chars)
{
  int i;

  printf("[%u] ", (int)chars.uCharSetType);
  for (i = 0; i < 63; i++) {
    if (chars.aCharSetInfo[i]) {
      printf("%c", chars.aCharSetInfo[i]);
    } else {
      i = 64;
    }
  }
  printf("\n");
}

int Is_Charspec(struct charspec chars)
{
  int i = 0;
  UINT8 ref[] = UDF_CHARSPEC;

  if (chars.uCharSetType) 
    return 0;
  i = 0;
  while (i < 63) {
    if (i < strlen(ref)) {
      if (ref[i] != chars.aCharSetInfo[i])
        return 0;
    } else {
      if (chars.aCharSetInfo[i])
        return 0;
    }
    i++;
  }
  return 1;
}


/********************************************************************/
/* Display an ISO/IEC 13346 timestamp structure                     */
/********************************************************************/
void printTimestamp( struct timestamp Time)
{
    /* assumes character is positioned, ends with newline */

    int tp = GetTSTP(U_endian16(Time.uTypeAndTimeZone));
    int tz = GetTSTZ(U_endian16(Time.uTypeAndTimeZone));



    printf("%4.4d/%2.2d/%2.2d ",
           U_endian16(Time.iYear), Time.uMonth, Time.uDay);
    printf("%2.2d:%2.2d:%2.2d.",
           Time.uHour, Time.uMinute, Time.uSecond);
    printf("%2.2d%2.2d%2.2d",
           Time.uCentiseconds,Time.uHundredMicroseconds,Time.uMicroseconds);
    printf(" (%s)",
           (tp == 0 ? "CUT" :
            (tp == 1 ? "Local" :
             "Non-ISO")));
    printf(", %d %s\n",
           tz,(tz == -2047 ? "(No timezone specified)" :
               (tz <= 1440 && tz >= -1440 ? "min. from CUT" :
                "(***INVALID timezone value***)")));
}

/********************************************************************/
/* Display a long_ad structure                                      */
/********************************************************************/
void printLongAd(struct long_ad *longad)
{
  printf("%d bytes @ %d:%d\n", U_endian32(longad->ExtentLength.Length32) & 0x3FFFFFFF, 
         U_endian16(longad->Location_PartNo), U_endian32(longad->Location_LBN));
}

