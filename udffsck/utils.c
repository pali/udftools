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
    printf("\tIdentification Tag\n"
           "\t==================\n");
    printf("\tID: %d (", id.tagIdent);
    switch(id.tagIdent) {
        case TAG_IDENT_PVD:
            printf("PVD");
            break;
        case TAG_IDENT_AVDP:
            printf("AVDP");
            break;
        case TAG_IDENT_VDP:
            printf("VDP");
            break;
        case TAG_IDENT_IUVD:
            printf("IUVD");
            break;
        case TAG_IDENT_PD:
            printf("PD");
            break;
        case TAG_IDENT_LVD:
            printf("LVD");
            break;
        case TAG_IDENT_USD:
            printf("USD");
            break;
        case TAG_IDENT_TD:
            printf("TD");
            break;    
        case TAG_IDENT_LVID:
            printf("LVID");
            break;
    }
    printf(")\n");
    printf("\tVersion: %d\n", id.descVersion);
    printf("\tChecksum: 0x%x\n", id.tagChecksum);
    printf("\tSerial Number: 0x%x\n", id.tagSerialNum);
    printf("\tDescriptor CRC: 0x%x, Length: %d\n", id.descCRC, id.descCRCLength);
    printf("\tTag Location: 0x%x\n", id.tagLocation);
}

int print_disc(struct udf_disc *disc) {
    printf("UDF Metadata Overview\n"
           "=====================\n");
    printf("UDF revision: %d\n", disc->udf_rev);
    printf("Disc blocksize: %d\n", disc->blocksize);
    printf("Disc blocksize bits: %d\n", disc->blocksize_bits);
    printf("Flags: %X\n\n", disc->flags);


    printf("AVDP\n"
           "----\n");
    for(int i=0; i<3; i++) {
        printf("[%d]\n", i);
        if(disc->udf_anchor[i] != 0) {
            read_tag(disc->udf_anchor[i]->descTag);
        }
    }
    
    printf("PVD\n"
           "---\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_pvd[i] != 0) {
            read_tag(disc->udf_pvd[i]->descTag);
        }
    }

    printf("LVD\n"
           "---\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_lvd[i] != 0) {
            read_tag(disc->udf_lvd[i]->descTag);
            printf("Partition Maps: %d\n",disc->udf_lvd[i]->partitionMaps[0]);
        }
    }

    printf("PD\n"
           "--\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_pd[i] != 0) {
            read_tag(disc->udf_pd[i]->descTag);
        }
    }

    printf("USD\n"
           "---\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_usd[i] != 0) {
            read_tag(disc->udf_usd[i]->descTag);
        }
    }

    printf("IUVD\n"
           "----\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
        if(disc->udf_iuvd[i] != 0) {
            read_tag(disc->udf_iuvd[i]->descTag);
        }
    }

    printf("TD\n"
           "--\n");
    for(int i=0; i<2; i++) {
        printf("[%d]\n", i);
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

void logger(message_type type, const char *format, va_list arg) {
	//va_list arg;
	//char *msg;
	char *prefix;
	char *color;
    FILE *stream;

	switch(type) {
		case debug:
			prefix = "DBG";
			color = "";
            stream = stdout;
			break;
		case message:
			prefix = "MSG";
			color = "";
            stream = stdout;
			break;
		case important:
			prefix = "MSG";
			color = ANSI_COLOR_GREEN;
            stream = stdout;
			break;
		case warning:
			prefix = "WARN";
			color = ANSI_COLOR_YELLOW;
            stream = stdout;
			break;
		case error:
			prefix = "ERROR";
			color = ANSI_COLOR_RED;
            stream = stderr;
			break;
		case faterr:
			prefix = "FATAL";
			color = ANSI_COLOR_RED;
            stream = stderr;
			break;
		default:
			prefix = 0;
			color = "";
            stream = stderr;
			break;
	}

	if(prefix > 0)
		fprintf(stream, "%s[%s] ", color, prefix);
	vfprintf (stream, format, arg);
	fprintf(stream, ANSI_COLOR_RESET EOL);
}

void dbg(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(debug, format, arg);
	va_end (arg);
}

void note(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(show, format, arg);
	va_end (arg);
}

void msg(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(message, format, arg);
	va_end (arg);
}

void imp(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(important, format, arg);
	va_end (arg);
}

void warn(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(warning, format, arg);
	va_end (arg);
}

void err(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(error, format, arg);
	va_end (arg);
}

void fatal(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(faterr, format, arg);
	va_end (arg);
}


void print_metadata_sequence(vds_sequence_t *seq) {
    printf("Main             Reserve\n");
    printf("ident | Errors | ident | Errors \n");     
    for(int i=0; i<VDS_STRUCT_AMOUNT; ++i) {
        printf("%5d |   0x%02x | %5d |   0x%02x \n", seq->main[i].tagIdent, seq->main[i].error, seq->reserve[i].tagIdent, seq->reserve[i].error);
    }
}
