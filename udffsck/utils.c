/*
 * utils.c
 *
 * Copyright (c) 2016    Vojtech Vladyka <vojtch.vladyka@gmail.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include "config.h"

#include "utils.h"

#include <stdio.h>
#include <stdarg.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define EOL ""

typedef enum {
	show = 0,
	message,
	important,
	warning,
	error,
	faterr,
    debug
} message_type;

verbosity_e verbosity;

void read_tag(tag id) {
    note("\tIdentification Tag\n"
           "\t==================\n");
    note("\tID: %d (", id.tagIdent);
    switch(id.tagIdent) {
        case TAG_IDENT_PVD:
            note("PVD");
            break;
        case TAG_IDENT_AVDP:
            note("AVDP");
            break;
        case TAG_IDENT_VDP:
            note("VDP");
            break;
        case TAG_IDENT_IUVD:
            note("IUVD");
            break;
        case TAG_IDENT_PD:
            note("PD");
            break;
        case TAG_IDENT_LVD:
            note("LVD");
            break;
        case TAG_IDENT_USD:
            note("USD");
            break;
        case TAG_IDENT_TD:
            note("TD");
            break;    
        case TAG_IDENT_LVID:
            note("LVID");
            break;
    }
    note(")\n");
    note("\tVersion: %d\n", id.descVersion);
    note("\tChecksum: 0x%x\n", id.tagChecksum);
    note("\tSerial Number: 0x%x\n", id.tagSerialNum);
    note("\tDescriptor CRC: 0x%x, Length: %d\n", id.descCRC, id.descCRCLength);
    note("\tTag Location: 0x%x\n", id.tagLocation);
}

int print_disc(struct udf_disc *disc) {
    note("\nUDF Metadata Overview\n"
          "---------------------\n");

    note("AVDP\n"
        "----\n");
    for(int i=0; i<3; i++) {
        note("[%d]\n", i);
        if(disc->udf_anchor[i] != 0) {
            read_tag(disc->udf_anchor[i]->descTag);
        }
    }
    
    note("PVD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_pvd[i] != 0) {
            read_tag(disc->udf_pvd[i]->descTag);
        }
    }

    note("LVD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_lvd[i] != 0) {
            read_tag(disc->udf_lvd[i]->descTag);
            note("\tPartition Maps: %d\n",disc->udf_lvd[i]->partitionMaps[0]);
        }
    }

    note("PD\n"
        "--\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_pd[i] != 0) {
            read_tag(disc->udf_pd[i]->descTag);
        }
    }

    note("USD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_usd[i] != 0) {
            read_tag(disc->udf_usd[i]->descTag);
            note("\tNumOfAllocDescs: %d\n", disc->udf_usd[i]->numAllocDescs);
        }
    }

    note("IUVD\n"
        "----\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_iuvd[i] != 0) {
            read_tag(disc->udf_iuvd[i]->descTag);
        }
    }

    note("TD\n"
        "--\n");
    for(int i=0; i<2; i++) {
        note("[%d]\n", i);
        if(disc->udf_td[i] != 0) {
            read_tag(disc->udf_td[i]->descTag);
        }
    }

    return 0;
}

int prompt(const char *format, ...) {
    va_list args;
    char b = 0,c = 0;
    char again = 0;

    do {
        again = 0;
        va_start(args, format);

        vprintf(format, args);

        va_end(args);

        c = getchar();
        while ((b=getchar()) != EOF && b != '\n');

        if(c == 'y' || c == 'Y') {
            return 1;
        } else if(c == 'n' || c == 'N') {
            return 0;
        } else if(c == '\n') {
            return -1;
        } else {
            again = 1;
        }
    } while(again);

    return -128;
}





/* Private function prototypes -----------------------------------------------*/
#ifdef __GNUC__
  /* With GCC/RAISONANCE, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

/* USER CODE END PFP */

void logger(message_type type, char *color, const char *format, va_list arg) {
	//va_list arg;
	//char *msg;
	char *prefix;
	//char *color;
    FILE *stream;
    verbosity_e verblvl;  

	switch(type) {
		case debug:
			prefix = "DBG";
//			color = "";
            stream = stdout;
            verblvl = DBG;
			break;
		case message:
			prefix = 0;
//			color = "";
            stream = stdout;
            verblvl = MSG;
			break;
		case important:
			prefix = 0;
//			color = ANSI_COLOR_GREEN;
            stream = stdout;
            verblvl = WARN;
			break;
		case warning:
			prefix = "WARN";
//			color = ANSI_COLOR_YELLOW;
            stream = stdout;
            verblvl = WARN;
			break;
		case error:
			prefix = "ERROR";
//			color = ANSI_COLOR_RED;
            stream = stderr;
            verblvl = NONE;
			break;
		case faterr:
			prefix = "FATAL";
//			color = ANSI_COLOR_RED;
            stream = stderr;
            verblvl = NONE;
			break;
		default:
			prefix = 0;
//			color = "";
            stream = stdout;
            verblvl = DBG;
			break;
	}

    if(verbosity >= verblvl) {
        if(color == NULL)
            color = "";
        if(prefix > 0)
            fprintf(stream, "%s[%s] ", color, prefix);
        else
            fprintf(stream, "%s", color);
        vfprintf (stream, format, arg);
        fprintf(stream, ANSI_COLOR_RESET EOL);
    }
}

void dbg(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(debug, "", format, arg);
	va_end (arg);
}

void dwarn(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(debug, ANSI_COLOR_YELLOW, format, arg);
	va_end (arg);
}

void note(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(show, "", format, arg);
	va_end (arg);
}

void msg(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(message, "", format, arg);
	va_end (arg);
}

void imp(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(important, ANSI_COLOR_GREEN, format, arg);
	va_end (arg);
}

void warn(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(warning, ANSI_COLOR_YELLOW, format, arg);
	va_end (arg);
}

void err(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(error, ANSI_COLOR_RED, format, arg);
	va_end (arg);
}

void fatal(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(faterr, ANSI_COLOR_RED, format, arg);
	va_end (arg);
}

char * verbosity_level_str(verbosity_e lvl) {
    
/*typedef enum {
    NONE=0,
    WARN,
    MSG,
    DBG
} verbosity_e;*/
    switch(lvl) {
        case NONE:
            return "NONE";
        case WARN:
            return "WARNING";
        case MSG:
            return "MESSAGE";
        case DBG:
            return "DEBUG";
        default: 
            return "UNKNOWN";
    }
}

void print_metadata_sequence(vds_sequence_t *seq) {
    note("Main             Reserve\n");
    note("ident | Errors | ident | Errors \n");     
    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        note("%5d |   0x%02x | %5d |   0x%02x \n", seq->main[i].tagIdent, seq->main[i].error, seq->reserve[i].tagIdent, seq->reserve[i].error);
    }
}
