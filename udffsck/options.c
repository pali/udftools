/*
 * options.c
 *
 * Copyright (c) 2016       Vojtech Vladyka <vojtech.vladyka@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "libudffs.h"
#include "options.h"
#include "utils.h"

verbosity_e verbose = NONE;
int interactive = 0;
int autofix = 0;

static struct option long_options[] =
{
    /* These options set a flag. */
    {"verbose", no_argument,  0, 'v'},
    {"blocksize",  required_argument, 0, 'B'},
    {"interactive",  no_argument, 0, 'i'},
    {"autofix",    no_argument, 0, 'p'},
    {"check", no_argument, 0, 'c'},
    {"help",    no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static char * help[] = {
    "Increase verbosity. Without it are printed only error messages, -v prints warnings, -vv is for humans, -vvv is for developers and curious people.",
    "Medium block size. Mandatory parameter, can be 512, 1024, 2048 or 4096.",
    "Medium is will be fixed interactivelly and all fixings must be authorized by user.",
    "Medium is will be fixed automatically. All found errors will be fixed if possible.",
    "Medium will be only checked. This is default behavior, but this flag override -p.",
    "This help message.",
    ""
}; 


void usage(void)
{
    int i;

    printf("udffsck " UDFFSCK_VERSION  " from " PACKAGE_NAME " " PACKAGE_VERSION ".");
    printf("\nUsage:\n\tudffsck [-icpvvvh] [-B blocksize] medium\n");
    printf("Options:\n");
    for (i = 0; long_options[i].name != NULL; i++) {
        if (long_options[i].flag != 0)
            printf("  --%s\t", long_options[i].name);
        else
            printf("  -%c\t", long_options[i].val);
        printf(" %s\n", help[i]);
    }
    printf("Return codes:\n");
    printf("  0 - No error\n"
           "  1 - Filesystem errors were fixed\n"
          /* "  2 - Filesystem errors were fixed, reboot is recomended\n"*/
           "  4 - Filesystem errors remained unfixed\n"
           "  8 - Program error\n"
           "  16 - Wrong input parameters\n"
           "  32 - Check was interrupted by user request\n"
          /* "  128 - Shared library error"*/
           "\n");
    exit(32);
}

void parse_args(int argc, char *argv[], char **path, int *blocksize) 
{
    int c;

    while (1)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "vB:ipch", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                printf ("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
                break;

            case 'B':
                *blocksize = strtol(optarg, NULL, 10);
                printf("Device block size: %d\n", *blocksize);
                break;

            case 'i':
                printf ("Medium will be fixed interactively. Expect questions.\n");
                interactive = 1;
                break;

            case 'p':
                printf ("We try to fix medium automaticaly.\n");
                autofix = 1;
                break;

            case 'c':
                printf ("Medium will be only checked. No corrections.\n");
                autofix = 0;
                break;

            case 'v':
                verbosity ++;
                if(verbosity > DBG)
                    verbosity = DBG;
                printf("Verbosity increased to %s.\n", verbosity_level_str(verbosity));
                break;

            case 'h':
                usage();
                break;

            default:
                printf("Unrecognized option -%c.\n", c);
                usage(); 
                break;
        }
    }

    /* Print any remaining command line arguments (not options). */
    if (optind < argc)
    {
        dbg("Optind: %d\n", optind);
        dbg("non-option ARGV-elements: ");
        while (optind < argc) { 
            *path = (char*)malloc(strlen(argv[optind])+1);
            strcpy(*path, argv[optind]);
            dbg("%s ", *path);
            optind++;
            if(optind > 2) //We accept one medium at a time. 
                break;
        }
        dbg("\n");
    }
    dbg("Param parse done.\n");
}

