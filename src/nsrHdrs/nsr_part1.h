#ifndef __NSRPART1H__
#define __NSRPART1H__

typedef UINT8 dstring;

/*
 * ----------- Definitions for basic structures ------------------
 */

/* [1/7.2.1] -----------------------------------------------------*/
struct charspec {
    UINT8 uCharSetType;
    UINT8 aCharSetInfo[63];
};
/* uCharSetType is one of: */
#define NSR_CS0  0
#define NSR_CS1  1
#define NSR_CS2  2
#define NSR_CS3  3
#define NSR_CS4  4
#define NSR_CS5  5
#define NSR_CS6  6
#define NSR_CS7  7
#define NSR_CS8  8

/* [1/7.3] -------------------------------------------------------*/
struct timestamp {
    INT16 uTypeAndTimeZone;
    INT16  iYear;
    UINT8 uMonth;
    UINT8 uDay;
    UINT8 uHour;
    UINT8 uMinute;
    UINT8 uSecond;
    UINT8 uCentiseconds;
    UINT8 uHundredMicroseconds;
    UINT8 uMicroseconds;
};
/* CUT = Coordinated Universal Time; LOCAL = local time; BA = By agreement */
#define TIMETYPE_CUT   0
#define TIMETYPE_LOCAL 1
#define TIMETYPE_BA    2

/* Note: using a 16 bit bitfield is not possible with some compilers */
#define TPMask 0xf000
#define TPShift 12

#define TZMask 0x0fff
#define TZSignBit 0x0800
#define TZSignExt 0xf000

#define GetTSTP(ttz) (ttz>>TPShift)
#define GetTSTZ(ttz) ((INT16)((ttz&TZSignBit)?(ttz|TZSignExt):(ttz&TZMask)))
#define SetTSTP(ttz,tp) ((tp<<TPShift) | (ttz&TZMask))
#define SetTSTZ(ttz,tz) ((ttz&TPMask) | (tz&TZMask))


/* [1/7.4] ISO-definition of regid -------------------------------*/
struct regid {
    UINT8 uFlags;
    UINT8 aRegisteredID[23];
    UINT8 aIDSuffix[8];
};

#define REGID_FLAGS_DIRTY     0x01
#define REGID_FLAGS_PROTECTED 0x02

/*
 * UDF1.01 / 2.1.4.2
 * This always contains "*OSTA UDF Compliant" in aID
 * and DOMAIN identifier suffix in the suffix area.
 * uUDFRevision = 0x0100
 * uDomainFlags usually = 0.
 */
struct domainEntityId {
    UINT8 uFlags;
    UINT8 aID[23];
    UINT16 uUDFRevision;
    UINT8 uDomainFlags;
    UINT8 aReserved[5];
};

/*
 * UDF1.01 / 2.1.4.2
 * This is used with implementation use identifiers that are defined in
 * UDF.  uUDF revision is as above.
 * uOSClass is defined in constants in nsr.h.
 * uOSIdentifier is defined in constants in nsr.h.
 */
struct udfEntityId {
    UINT8 uFlags;
    UINT8 aID[23];
    UINT16 uUDFRevision;
    UINT8 uOSClass;
    UINT8 uOSIdentifier;
    UINT8 aReserved[4];
};

/* 
 * UDF1.01 / 2.1.4.2
 * This type of identifier contains the name of the implementation that
 * generated the structure
 */

struct implEntityId {
    UINT8 uFlags        ;
    UINT8 aID[23]       ;
    UINT8 uOSClass      ;
    UINT8 uOSIdentifier ;
    UINT8 uImplUse[6]   ;
};

/* 
 * UDF1.01 / 2.1.4.2
 * This type of identifier contains the name of the implementation that
 * generated the structure (HP specific)
 */

struct HPimplEntityId {
    UINT8 uFlags        ;
    UINT8 aID[23]       ;
    UINT8 uOSClass      ;
    UINT8 uOSIdentifier ;
    UINT8 uDescVersion  ;
    UINT8 uLibrVersion  ;
    UINT16 uImplRegNo   ;
    UINT8 uImplVersion  ;
    UINT8 uImplRevision ;
};


#endif
