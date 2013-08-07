/* ------------------------------------------------------------------------- */
/* Copyright 2013 Esri                                                       */
/*                                                                           */
/* Licensed under the Apache License, Version 2.0 (the "License");           */
/* you may not use this file except in compliance with the License.          */
/* You may obtain a copy of the License at                                   */
/*                                                                           */
/*     http://www.apache.org/licenses/LICENSE-2.0                            */
/*                                                                           */
/* Unless required by applicable law or agreed to in writing, software       */
/* distributed under the License is distributed on an "AS IS" BASIS,         */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  */
/* See the License for the specific language governing permissions and       */
/* limitations under the License.                                            */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* This program will dump and/or copy GEOCON files with optional extent      */
/* processing.                                                               */
/* ------------------------------------------------------------------------- */

#if _WIN32
#  pragma warning (disable: 4996) /* _CRT_SECURE_NO_WARNINGS */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "libgeocon.h"

/*------------------------------------------------------------------------
 * program options and variables
 */
static const char *    pgm       = GEOCON_NULL;
static const char *    outfile   = GEOCON_NULL;            /* -o file      */

static GEOCON_BOOL     dump_hdr  = FALSE;                  /* -h           */
static GEOCON_BOOL     list_hdr  = FALSE;                  /* -l           */
static GEOCON_BOOL     dump_data = FALSE;                  /* -d           */
static GEOCON_BOOL     read_data = FALSE;                  /* -d | -o file */
static GEOCON_EXTENT   extent    = { 0 };                  /* -e ...       */
static GEOCON_EXTENT * extptr    = GEOCON_NULL;            /* -e ...       */
static int             endian    = GEOCON_ENDIAN_INP_FILE; /* -B | -L | -N */

static GEOCON_BOOL     do_title  = TRUE;

/*------------------------------------------------------------------------
 * display usage
 */
static void display_usage(int level)
{
   if ( level )
   {
      printf("Usage: %s [options] file ...\n", pgm);
      printf("Options:\n");
      printf("  -?, -help  Display help\n");
      printf("\n");

      printf("  -l         List header info\n");
      printf("  -h         Dump header info\n");
      printf("  -d         Dump shift data\n");
      printf("\n");

      printf("  -B         Write    big-endian binary file\n");
      printf("  -L         Write little-endian binary file\n");
      printf("  -N         Write native-endian binary file\n");
      printf("               (default is same as input file)\n");
      printf("\n");

      printf("  -o file    Specify output file\n");
      printf("  -e slat wlon nlat elon   Specify extent\n");
   }
   else
   {
      fprintf(stderr,
         "Usage: %s [-h|-l] [-d] [-B|-L|-N] [-o file]\n",
         pgm);
      fprintf(stderr,
         "       %*s [-e slat wlon nlat elon] file ...\n",
         (int)strlen(pgm), "");
   }
}

/*------------------------------------------------------------------------
 * process command-line options
 */
static int process_options(int argc, const char **argv)
{
   int optcnt;

   if ( pgm == GEOCON_NULL )  pgm = strrchr(argv[0], '/');
   if ( pgm == GEOCON_NULL )  pgm = strrchr(argv[0], '\\');
   if ( pgm == GEOCON_NULL )  pgm = argv[0];
   else                       pgm++;

   memset(&extent, 0, sizeof(extent));

   for (optcnt = 1; optcnt < argc; optcnt++)
   {
      const char *arg = argv[optcnt];

      if ( *arg != '-' )
         break;

      while (*arg == '-') arg++;

      if ( strcmp(arg, "?")    == 0 ||
           strcmp(arg, "help") == 0 )
      {
         display_usage(1);
         exit(EXIT_SUCCESS);
      }

      else if ( strcmp(arg, "l") == 0 ) list_hdr   = TRUE;
      else if ( strcmp(arg, "h") == 0 ) dump_hdr   = TRUE;
      else if ( strcmp(arg, "d") == 0 ) dump_data  = TRUE;

      else if ( strcmp(arg, "B") == 0 ) endian     = GEOCON_ENDIAN_BIG;
      else if ( strcmp(arg, "L") == 0 ) endian     = GEOCON_ENDIAN_LITTLE;
      else if ( strcmp(arg, "N") == 0 ) endian     = GEOCON_ENDIAN_NATIVE;

      else if ( strcmp(arg, "o") == 0 )
      {
         if ( ++optcnt >= argc )
         {
            fprintf(stderr, "%s: option needs an argument -- -%s\n",
               pgm, "o");
            display_usage(0);
            exit(EXIT_FAILURE);
         }
         outfile   = argv[optcnt];
      }

      else if ( strcmp(arg, "e") == 0 )
      {
         if ( (optcnt+4) >= argc )
         {
            fprintf(stderr, "%s: option needs 4 arguments -- -%s\n",
               pgm, "e");
            display_usage(0);
            exit(EXIT_FAILURE);
         }
         extent.slat = atof( argv[++optcnt] );
         extent.wlon = atof( argv[++optcnt] );
         extent.nlat = atof( argv[++optcnt] );
         extent.elon = atof( argv[++optcnt] );
         extptr = &extent;
      }

      else
      {
         fprintf(stderr, "Invalid option -- %s\n", argv[optcnt]);
         display_usage(0);
         exit(EXIT_FAILURE);
      }
   }

   read_data = ( dump_data || (outfile != GEOCON_NULL) );

   if ( argc == optcnt )
   {
      fprintf(stderr, "%s: No files specified.\n", pgm);
      display_usage(0);
      exit(EXIT_FAILURE);
   }

   if ( list_hdr && dump_hdr )
   {
      fprintf(stderr, "%s: Both -l and -h specified. -h ignored.\n",
         pgm);
      dump_hdr  = FALSE;
   }

   if ( list_hdr && dump_data )
   {
      fprintf(stderr, "%s: Both -l and -d specified. -d ignored.\n",
         pgm);
      dump_data = FALSE;
   }

   if ( outfile != GEOCON_NULL && dump_data )
   {
      fprintf(stderr, "%s: Both -o and -d specified. -d ignored.\n", pgm);
      dump_data = FALSE;
   }

   if ( outfile != GEOCON_NULL && (optcnt+1) < argc )
   {
      fprintf(stderr, "%s: Too many files specified.\n",
         pgm);
      display_usage(0);
      exit(EXIT_FAILURE);
   }

   return optcnt;
}

/*------------------------------------------------------------------------
 * process a GEOCON file
 */
static int process_file(const char *inpfile)
{
   GEOCON_HDR * hdr;
   int gcerr;
   int rc = 0;

   /* Load the file. */

   hdr = geocon_load(
      inpfile,            /* in:  filename         */
      extptr,             /* in:  extent pointer?  */
      read_data,          /* in:  read shift data? */
      &gcerr);            /* out: error code       */
   if ( hdr == GEOCON_NULL )
   {
      char msg_buf[GEOCON_MAX_ERR_LEN];
      printf("%s: cannot read input file: %s\n",
         inpfile, geocon_errmsg(gcerr, msg_buf));
      return -1;
   }

   /* Dump the header and/or data if requested. */

   if ( list_hdr  ) geocon_list_hdr (hdr, stdout, do_title),
                    do_title = FALSE;
   if ( dump_hdr  ) geocon_dump_hdr (hdr, stdout);
   if ( dump_data ) geocon_dump_data(hdr, stdout);

   /* Write out a new file if requested. */

   if ( outfile != GEOCON_NULL )
   {
      rc = geocon_write(hdr, outfile, endian, &gcerr);
      if ( rc != GEOCON_ERR_OK )
      {
         char msg_buf[GEOCON_MAX_ERR_LEN];
         printf("%s: Cannot write output file: %s\n",
            outfile, geocon_errmsg(gcerr, msg_buf));
      }
   }

   geocon_delete(hdr);
   return (rc == GEOCON_ERR_OK) ? 0 : -1;
}

/*------------------------------------------------------------------------
 * main
 */
int main(int argc, const char **argv)
{
   int optcnt = process_options(argc, argv);
   int rc = 0;

   while (optcnt < argc)
   {
      rc |= process_file(argv[optcnt++]);
   }

   return (rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
