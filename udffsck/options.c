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

/*
   struct option long_options[] = {
   { "help", no_argument, NULL, OPT_HELP },
   { "version", no_argument, NULL, OPT_HELP },
   { 0, 0, NULL, 0 },
   };
   */
static int verbose_flag;

static struct option long_options[] =
{
    /* These options set a flag. */
    {"verbose", no_argument,       &verbose_flag, 1},
    {"brief",   no_argument,       &verbose_flag, 0},
    /* These options don’t set a flag.
     *              We distinguish them by their indices. */
    {"add",     no_argument,       0, 'a'},
    {"append",  no_argument,       0, 'b'},
    {"delete",  required_argument, 0, 'd'},
    {"create",  required_argument, 0, 'c'},
    {"file",    required_argument, 0, 'f'},
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

void parse_args(int argc, char *argv[],  char **path) 
{
    int c;

    while (1)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "abhc:d:f:", long_options, &option_index);

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

            case 'a':
                puts ("option -a\n");
                break;

            case 'b':
                puts ("option -b\n");
                break;

            case 'c':
                printf ("option -c with value `%s'\n", optarg);
                break;

            case 'd':
                printf ("option -d with value `%s'\n", optarg);
                break;

            case 'f':
                printf ("option -f with value `%s'\n", optarg);
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

    /* Instead of reporting ‘--verbose’
     *      and ‘--brief’ as they are encountered,
     *           we report the final status resulting from them. */
    if (verbose_flag)
        puts ("verbose flag is set");

    /* Print any remaining command line arguments (not options). */
    if (optind < argc)
    {
        printf ("non-option ARGV-elements: ");
        while (optind < argc) { //TODO deal with other unrecognized params somehow...
            *path = (char*)malloc(strlen(argv[optind]));
            strcpy(*path, argv[optind]);
            printf ("%s ", *path);
            optind++;
        }
        putchar ('\n');
    }
}

