/*
 * options.c
 *
 * Copyright (c) 2002       Ben Fennema <bfennema@falcon.csc.calpoly.edu>
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

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "libudffs.h"
#include "options.h"
#include "utils.h"
/*
   struct option long_options[] = {
   { "help", no_argument, NULL, OPT_HELP },
   { "version", no_argument, NULL, OPT_HELP },
   { 0, 0, NULL, 0 },
   };
   */

verbosity_e verbose = NONE;
int interactive = 0;
int autofix = 0;

static struct option long_options[] =
{
    /* These options set a flag. */
    {"verbose", no_argument,  0, 'v'},
    {"blocksize",  required_argument, 0, 'b'},
    {"interactive",  no_argument, 0, 'i'},
    {"fix",    no_argument, 0, 'f'},
    {"check", no_argument, 0, 'c'},
    {"help",    no_argument,       0, 'h'},
    {0, 0, 0, 0}
};



void usage(void)
{
    int i;

    printf("udffsck from " PACKAGE_NAME " " PACKAGE_VERSION "\nUsage:\n\tudffsck [options] \nOptions:\n");
    for (i = 0; long_options[i].name != NULL; i++)
        if (long_options[i].flag != 0)
            printf("\t--%s\t\t%s\n", long_options[i].name, long_options[i].name);
        else
            printf("\t-%c\t\t%s\n", long_options[i].val, long_options[i].name);
    exit(1);
}

void parse_args(int argc, char *argv[], char **path, int *blocksize) 
{
    int c;

    while (1)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "vb:ifch", long_options, &option_index);

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

            case 'b':
                *blocksize = strtol(optarg, NULL, 10);
                printf("Device block size: %d\n", *blocksize);
                break;

            case 'i':
                printf ("Medium will be fixed interactively. Expect questions.\n");
                interactive = 1;
                break;

            case 'f':
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

            case '?':
                /* getopt_long already printed an error message. */
                break;

            default:
                abort ();
        }
    }

    /* Print any remaining command line arguments (not options). */
    if (optind < argc)
    {
        dbg("non-option ARGV-elements: ");
        dbg("Optind: %d\n", optind);
        while (optind < argc) { //TODO deal with other unrecognized params somehow...
            *path = (char*)malloc(strlen(argv[optind])+1);
            strcpy(*path, argv[optind]);
            printf ("%s ", *path);
            optind++;
        }
        putchar ('\n');
    }
    dbg("Param parse done.\n");
}

