/*
 * Copyright (C) 2017 Vojtech Vladyka <vojtech.vladyka@gmail.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

#include "log.h"
#include "options.h"

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

/**
 * \brief Simple prompt printing out message and accepting y/Y/n/N. Anything else restarts prompt.
 *
 * \param[in] *format formatting string with params for vprintf()
 *
 * \return 0 if n/N
 * \return 1 if y/Y
 * \return -1 if CRLF
 * \return -128 prompt failed
 */
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
        do {
            b = getchar();
        } while (b != EOF || b != '\n');

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

/**
 * \brief Internall logger function producing printing to stdout
 *
 * \param[in] type mesage types are debug, message, important, warning, error, faterr
 * \param[in] *color color ASCII formating string
 * \param[in] *format message to print
 * \param[in] arg aguments to message
 */
void logger(message_type type, char *color, const char *format, va_list arg) {
	char *prefix;
    FILE *stream;
    verbosity_e verblvl;  

	switch(type) {
		case debug:
			prefix = "DBG";
            stream = stdout;
            verblvl = DBG;
			break;
		case message:
			prefix = 0;
            stream = stdout;
            verblvl = MSG;
			break;
		case important:
			prefix = 0;
            stream = stdout;
            verblvl = WARN;
			break;
		case warning:
			prefix = "WARN";
            stream = stdout;
            verblvl = WARN;
			break;
		case error:
			prefix = "ERROR";
            stream = stderr;
            verblvl = NONE;
			break;
		case faterr:
			prefix = "FATAL";
            stream = stderr;
            verblvl = NONE;
			break;
		default:
			prefix = 0;
            stream = stdout;
            verblvl = DBG;
			break;
	}

    if(verbosity >= verblvl) {
        if(color == NULL || colored == 0)
            color = "";
        if(prefix != NULL)
            fprintf(stream, "%s[%s] ", color, prefix);
        else
            fprintf(stream, "%s", color);
        vfprintf (stream, format, arg);
        if(colored == 1)
            fprintf(stream, ANSI_COLOR_RESET EOL);
    }
}

/**
 * \brief Debug output
 *
 * Prefix: **[DBG]**\n
 * Color: **default**\n
 * Output: **stdout**\n
 *
 * \param[in] *format string to print
 */
void dbg(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(debug, "", format, arg);
	va_end (arg);
}

/**
 * \brief Debug warning output
 *
 * Prefix: **[DBG]**\n
 * Color: **yellow**\n
 * Output: **stdout**\n
 *
 * \param[in] *format string to print
 */
void dwarn(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(debug, ANSI_COLOR_YELLOW, format, arg);
	va_end (arg);
}

/**
 * \brief Note output
 *
 * Prefix: ---\n
 * Color: **default**\n
 * Output: **stdout**\n
 *
 * \param[in] *format string to print
 */
void note(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(show, "", format, arg);
	va_end (arg);
}

/**
 * \brief Message output
 *
 * Prefix: ---\n
 * Color: **default**\n
 * Output: **stdout**\n
 *
 * \param[in] *format string to print
 */
void msg(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(message, "", format, arg);
	va_end (arg);
}

/**
 * \brief Important message output
 *
 * Prefix: ---\n
 * Color: **Green**\n
 * Output: **stdout**\n
 *
 * \param[in] *format string to print
 */
void imp(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(important, ANSI_COLOR_GREEN, format, arg);
	va_end (arg);
}

/**
 * \brief Warning output
 *
 * Prefix: **[WARN]**\n
 * Color: **Yellow**\n
 * Output: **stdout**\n
 *
 * \param[in] *format string to print
 */
void warn(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(warning, ANSI_COLOR_YELLOW, format, arg);
	va_end (arg);
}

/**
 * \brief Error output
 *
 * Prefix: **[ERR]**\n
 * Color: **Red**\n
 * Output: **stderr**\n
 *
 * \param[in] *format string to print
 */
void err(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(error, ANSI_COLOR_RED, format, arg);
	va_end (arg);
}

/**
 * \brief Fatal Error output
 *
 * Prefix: **[FATAL]**\n
 * Color: **Red**
 * Output: **stderr**
 *
 * \param[in] *format string to print
 */
void fatal(const char *format, ...) {
	va_list arg;
	va_start (arg, format);
	logger(faterr, ANSI_COLOR_RED, format, arg);
	va_end (arg);
}

/**
 * \brief Verbosity level to string
 *
 * \return constant char array
 */
char * verbosity_level_str(verbosity_e lvl) {
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
