// Uncomment one of the following:


// #define DOS
// #define WIN16
// #define WIN32
// #define OS2
// #define LINUX
#define SOLARIS

/* 
 *  The following adjust for byte order on various machines and interfaces.
 *  All structures in UDF are little endian, though the compressed unicode
 *  algorithm makes 16 bit values appear to be big endian.  The Following
 *  defines determine whether to swap the bytes or not for values read from
 *  S_: the SCSI interface
 *  U_: UDF structures.
 */

#ifdef LINUX
#include <endian.h>
#endif

#ifdef SOLARIS
#define __BYTE_ORDER 4321
#endif

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN 4321
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN

/* For little endian machines */
#define S_endian32(x) endian32(x)
#define S_endian16(x) endian16(x)
#define U_endian32(x) (x)
#define U_endian16(x) (x)

#else /* __BYTE_ORDER == __BIG_ENDIAN */

/* For big endian machines */

#define S_endian32(x) (x)
#define S_endian16(x) (x)
#define U_endian32(x) endian32(x)
#define U_endian16(x) endian16(x)

#endif

/* Shouldn't have to touch anything below here. */

/*
 * Each type below is OS and compiler specific.  
 *  INTn values must be signed and have exactly n bits.
 *  UINTn values must be unsigned and have exactly n bits.
 *  MINTn values must be signed and have at least n bits, but may have
 *           more for efficiency (i.e. system native int size)
 *  MUINTn values bust be unsigned and have at least n bits, but may have
 *           more for efficiency (i.e. system native int size)
 */

#ifdef DOS
typedef int            BOOL;

typedef char           INT8;
typedef unsigned char  UINT8;
typedef int            INT16;
typedef unsigned int   UINT16;
typedef long           INT32;
typedef unsigned long  UINT32;
typedef struct __uint64 {
   unsigned long       loword;
   unsigned long       hiword;
} UINT64;

typedef int            MINT8;
typedef unsigned int   MUINT8;
typedef int            MINT16;
typedef unsigned int   MUINT16;
typedef long           MINT32;
typedef unsigned long  MUINT32;

#endif

#ifdef WIN16
typedef int            BOOL;

typedef char           INT8;
typedef unsigned char  UINT8;
typedef int            INT16;
typedef unsigned int   UINT16;
typedef long           INT32;
typedef unsigned long  UINT32;
typedef struct __uint64 {
   unsigned long       loword;
   unsigned long       hiword;
} UINT64;

typedef int            MINT8;
typedef unsigned int   MUINT8;
typedef int            MINT16;
typedef unsigned int   MUINT16;
typedef long           MINT32;
typedef unsigned long  MUINT32;
#endif

#ifdef WIN32
typedef int            BOOL;

typedef char           INT8;
typedef unsigned char  UINT8;
typedef int            INT16;
typedef unsigned int   UINT16;
typedef long           INT32;
typedef unsigned long  UINT32;
typedef struct __uint64 {
   unsigned long       loword;
   unsigned long       hiword;
} UINT64;

typedef int            MINT8;
typedef unsigned int   MUINT8;
typedef int            MINT16;
typedef unsigned int   MUINT16;
typedef long           MINT32;
typedef unsigned long  MUINT32;
#endif

#ifdef OS2
typedef int            BOOL;

typedef char           INT8;
typedef unsigned char  UINT8;
typedef short          INT16;
typedef unsigned short UINT16;
typedef int            INT32;
typedef unsigned int   UINT32;
typedef struct __uint64 {
   unsigned long       loword;
   unsigned long       hiword;
} UINT64;

typedef int            MINT8;
typedef unsigned int   MUINT8;
typedef int            MINT16;
typedef unsigned int   MUINT16;
typedef int            MINT32;
typedef unsigned int   MUINT32;
#endif
 
#if defined(LINUX) || defined(SOLARIS)
typedef int            BOOL;

typedef char           INT8;
typedef unsigned char  UINT8;
typedef short          INT16;
typedef unsigned short UINT16;
typedef int            INT32;
typedef unsigned int   UINT32;
typedef struct __uint64 {
   long long       loword;
} UINT64;

typedef int            MINT8;
typedef unsigned int   MUINT8;
typedef int            MINT16;
typedef unsigned int   MUINT16;
typedef int            MINT32;
typedef unsigned int   MUINT32;
#endif
