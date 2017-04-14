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

int64_t udf_lseek64(int fd, int64_t offset, int whence) {
#if defined(HAVE_LSEEK64)
	return lseek64(fd, offset, whence);
#elif defined(HAVE_LLSEEK)
	return llseek(fd, offset, whence);
#else
	return lseek(fd, offset, whence);
#endif
}


void read_tag(tag id) {
    msg("\tIdentification Tag\n"
           "\t==================\n");
    msg("\tID: %d (", id.tagIdent);
    switch(id.tagIdent) {
        case TAG_IDENT_PVD:
            msg("PVD");
            break;
        case TAG_IDENT_AVDP:
            msg("AVDP");
            break;
        case TAG_IDENT_VDP:
            msg("VDP");
            break;
        case TAG_IDENT_IUVD:
            msg("IUVD");
            break;
        case TAG_IDENT_PD:
            msg("PD");
            break;
        case TAG_IDENT_LVD:
            msg("LVD");
            break;
        case TAG_IDENT_USD:
            msg("USD");
            break;
        case TAG_IDENT_TD:
            msg("TD");
            break;    
        case TAG_IDENT_LVID:
            msg("LVID");
            break;
    }
    msg(")\n");
    msg("\tVersion: %d\n", id.descVersion);
    msg("\tChecksum: 0x%x\n", id.tagChecksum);
    msg("\tSerial Number: 0x%x\n", id.tagSerialNum);
    msg("\tDescriptor CRC: 0x%x, Length: %d\n", id.descCRC, id.descCRCLength);
    msg("\tTag Location: 0x%x\n", id.tagLocation);
}

int print_disc(struct udf_disc *disc) {
    msg("UDF Metadata Overview\n"
           "=====================\n");
    msg("UDF revision: %d\n", disc->udf_rev);
    msg("Disc blocksize: %d\n", disc->blocksize);
    msg("Disc blocksize bits: %d\n", disc->blocksize_bits);
    msg("Flags: %X\n\n", disc->flags);


    msg("AVDP\n"
        "----\n");
    for(int i=0; i<3; i++) {
        msg("[%d]\n", i);
        if(disc->udf_anchor[i] != 0) {
            read_tag(disc->udf_anchor[i]->descTag);
        }
    }
    
    msg("PVD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        msg("[%d]\n", i);
        if(disc->udf_pvd[i] != 0) {
            read_tag(disc->udf_pvd[i]->descTag);
        }
    }

    msg("LVD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        msg("[%d]\n", i);
        if(disc->udf_lvd[i] != 0) {
            read_tag(disc->udf_lvd[i]->descTag);
            msg("\tPartition Maps: %d\n",disc->udf_lvd[i]->partitionMaps[0]);
        }
    }

    msg("PD\n"
        "--\n");
    for(int i=0; i<2; i++) {
        msg("[%d]\n", i);
        if(disc->udf_pd[i] != 0) {
            read_tag(disc->udf_pd[i]->descTag);
        }
    }

    msg("USD\n"
        "---\n");
    for(int i=0; i<2; i++) {
        msg("[%d]\n", i);
        if(disc->udf_usd[i] != 0) {
            read_tag(disc->udf_usd[i]->descTag);
            msg("\tNumOfAllocDescs: %d\n", disc->udf_usd[i]->numAllocDescs);
        }
    }

    msg("IUVD\n"
        "----\n");
    for(int i=0; i<2; i++) {
        msg("[%d]\n", i);
        if(disc->udf_iuvd[i] != 0) {
            read_tag(disc->udf_iuvd[i]->descTag);
        }
    }

    msg("TD\n"
        "--\n");
    for(int i=0; i<2; i++) {
        msg("[%d]\n", i);
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
            verblvl = MSG;
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
