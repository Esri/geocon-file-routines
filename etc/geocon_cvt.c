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
/* This program uses a GEOCON file to transform (forward or inverse)         */
/* lat/lon/hgt points from one datum to another.                             */
/* ------------------------------------------------------------------------- */

#if _WIN32
#  pragma warning (disable: 4996) /* _CRT_SECURE_NO_WARNINGS */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include "libgeocon.h"

/*------------------------------------------------------------------------
 * program options and variables
 */
static const char *    pgm         = GEOCON_NULL;
static const char *    filename    = GEOCON_NULL;        /* filename       */
static const char *    datafile    = "-";                /* -p file        */
static const char *    separator   = " ";                /* -s str         */

static GEOCON_BOOL     direction   = GEOCON_CVT_FORWARD; /* -f | -i        */
static GEOCON_BOOL     reversed    = FALSE;              /* -r             */
static GEOCON_BOOL     read_on_fly = FALSE;              /* -d             */
static GEOCON_BOOL     round_trip  = FALSE;              /* -R             */
static GEOCON_BOOL     interp_all  = FALSE;              /* -A             */
static GEOCON_BOOL     do_8086     = FALSE;              /* -k             */

static GEOCON_EXTENT   extent      = { 0 };              /* -e ...         */
static GEOCON_EXTENT * extptr      = GEOCON_NULL;        /* -e ...         */

static double          deg_factor  = 1.0;                /* -c deg-factor  */
static double          hgt_factor  = 1.0;                /* -h hgt-factor  */

static int             interp      = GEOCON_INTERP_DEFAULT;
                                                         /* -L, -C, -Q, -N */

static char            decimal_pt  = '.';        /* locale's decimal point */

/*------------------------------------------------------------------------
 * Cache for input *80* and *86* records
 *
 * Quoted comments below are taken directly from the file
 *    "http://beta.ngs.noaa.gov/operate.pdf".
 *
 * "These records are standard ASCII text with fixed column formatting.
 * They are a legacy format more fully described in Chapter 2, Horizontal
 * Observation (HZTL OBS) Data, of the Input Formats and Specifications of
 * the National Geodetic Survey Data Base 8 (the NGS Blue Book), Volume I -
 * Horizontal Control. This information is available online at
 * http://www.ngs.noaa.gov/FGCS/BlueBook/" (The all-caps below are theirs).
 *
 * Format of Blue Book *80* Control Point Record:
 *
 *    CC 01-06 SEQUENCE NUMBER. OPTIONAL. RIGHT JUSTIFIED.
 *             INCREMENT BY 10 FROM THE PREVIOUS RECORD.
 *    CC 07-10 DATA CODE. MUST BE *80*.
 *    CC 11-14 SSN. SEE PAGES 1-1, JOB CODE AND SURVEY POINT NUMBERING AND
 *             2-12, ASSIGNMENT OF STATION SERIAL NUMBERS.
 *    CC 15-44 STATION NAME. MUST NOT EXCEED 30 CHARACTERS. THE NAME OF A
 *             HORIZONTAL CONTROL POINT WITH PERIPHERAL REFERENCE MARKS
 *             AND/OR AZIMUTH MARKS MUST NOT EXCEED 24 CHARACTERS TO ALLOW
 *             FOR ADDING RM 1, RM 2, AND/OR AZ MK TO THE NAME WITHOUT
 *             EXCEEDING THE 30-CHARACTER LENGTH LIMIT.
 *    CC 45-55 LATITUDE. DEGREES, MINUTES, SECONDS (DDMMSSsssss).
 *    CC    56 DIRECTION OF LATITUDE. RECORD CODE "N" FOR NORTH OR CODE
 *             "S" FOR SOUTH.
 *    CC 57-68 LONGITUDE. DEGREES, MINUTES, SECONDS, (DDDMMSSsssss).
 *    CC    69 DIRECTION OF LONGITUDE. RECORD CODE "E" FOR EAST OR CODE
 *             "W" FOR WEST.
 *
 *             THE *86* RECORD IS TO BE USED FOR THE ELEVATION (ORTHOMETRIC
 *             HEIGHT) AND ELEVATION CODE, WHICH WERE FORMERLY DISPLAYED IN
 *             THE FOLLOWING TWO FIELDS.
 *
 *    CC 70-75 BLANK.
 *    CC    76 BLANK.
 *    CC 77-78 STATE OR COUNTRY CODE. IF THE CONTROL STATE IS LOCATED IN
 *             THE UNITED STATES/CANADA, ENTER THE CODE FROM ANNEX A FOR
 *             THE STATE/PROVINCE OR TERRITORY WHICH CONTAINS THE STATION.
 *             IF NOT, ENTER THE CODE FROM ANNEX A FOR THE COUNTRY WHICH
 *             CONTAINS THE STATION. SEE ANNEX A.
 *    CC 79-80 STATION ORDER AND TYPE. REFER TO PAGES 2-35 THROUGH 2-38,
 *             STATION ORDER AND TYPE AND SEE ANNEX E.
 *
 *    "GEOCON only considers columns 7 through 10 and 45 through 69 of this
 *    format. It is possible to not fill the remaining fields at all, or to
 *    fill them with alternative information. However, it will be difficult
 *    or impossible to interpret transformation quality and any supplemental
 *    vertical notification messages without a Station Serial Number (SSN)
 *    [Columns 11-14]."
 *
 * Format of Blue Book *86* Orthometric Height, Geoid Height, Ellipsoid Height:
 *
 *    CC 01-06 SEQUENCE NUMBER. OPTIONAL. RIGHT JUSTIFIED.
 *             INCREMENT BY 10 FROM THE PREVIOUS RECORD.
 *    CC 07-10 DATA CODE. MUST BE *86*.
 *    CC 11-14 SSN OF CONTROL POINT.
 *    CC 15-16 BLANK
 *    CC 17-23 ORTHOMETRIC HEIGHT. IN METERS (MMMMmmm).
 *    CC    24 ORTHOMETRIC HEIGHT CODE. SEE FOLLOWING TABLES.
 *    CC 25-26 ORTHOMETRIC HEIGHT ORDER AND CLASS.
 *             USE PUBLISHED VERTICAL ORDER AND CLASS, OTHERWISE LEAVE BLANK.
 *    CC    27 ORTHOMETRIC HEIGHT NGSIDB INDICATOR. SEE FOLLOWING TABLES.
 *    CC 28-29 ORTHOMETRIC HEIGHT DATUM. SEE FOLLOWING TABLES.
 *    CC 30-35 ORGANIZATION WHICH ESTABLISHED AND/OR MAINTAINS THE
 *             ORTHOMETRIC HEIGHT OF THE CONTROL POINT. ENTER THE
 *             ABBREVIATION LISTED IN ANNEX C OR ON THE DATASET
 *             IDENTIFICATION RECORD.
 *    CC 36-42 GEOID HEIGHT. ABOVE (POSITIVE) OR BELOW (NEGATIVE) THE
 *             REFERENCE ELLIPSOID. IN METERS (MMMMmmm).
 *    CC    43 GEOID HEIGHT CODE. SEE FOLLOWING TABLES.
 *    CC 44-45 BLANK.
 *    CC 46-52 ELLIPSOID HEIGHT. IN METERS (MMMMmmm).
 *    CC    53 ELLIPSOID HEIGHT CODE. SEE FOLLOWING TABLES.
 *    CC 54-55 ELLIPSOID HEIGHT ORDER AND CLASS. SEE ANNEX G.
 *    CC    56 ELLIPSOID HEIGHT DATUM. SEE TABLE, P. 2-85.
 *    CC 57-80 COMMENTS.
 *
 *    "GEOCON only considers columns 7 through 10 and 46 through 52 of this
 *    format. It is possible to not fill the remaining fields at all, or to
 *    fill them with alternative information. However, it will be difficult
 *    or impossible to interpret transformation quality and any supplemental
 *    vertical notification messages without a Station Serial Number (SSN)
 *    [Columns 11-14]."
 */
static char card_80[128];
static char card_86[128];

/*------------------------------------------------------------------------
 * output usage
 */
static void display_usage(int level)
{
   if ( level )
   {
      printf("Usage: %s [options] filename [lat lon hgt] ...\n", pgm);
      printf("Options:\n");
      printf("  -?, -help  Display help\n");
      printf("  -r         Reversed data: "
                           "(lon lat hgt) instead of (lat lon hgt)\n");
      printf("  -k         Read and write *80*/*86* records\n");
      printf("  -d         Read shift data on the fly (no load of data)\n");
      printf("  -f         Forward transformation           (default)\n");
      printf("  -i         Inverse transformation\n");
      printf("  -R         Do round trip\n");
      printf("\n");

      printf("  -L         Use bilinear       interpolation\n");
      printf("  -C         Use bicubic        interpolation\n");
      printf("  -N         Use natural spline interpolation\n");
      printf("  -Q         Use biquadratic    interpolation (default)\n");
      printf("  -A         Use all interpolation methods\n");
      printf("\n");

      printf("  -c value   Conversion: degrees-per-unit     "
                           "(default is %.17g)\n", deg_factor);
      printf("  -h value   Conversion: meters-per-unit      "
                           "(default is %.17g)\n", hgt_factor);
      printf("  -s string  Use string as output separator   "
                           "(default is \" \")\n");
      printf("  -p file    Read points from file            "
                           "(default is \"-\" or stdin)\n");
      printf("  -e slat wlon nlat elon   Specify an extent\n");
      printf("\n");

      printf("If no coordinate triples are specified on the command line,\n");
      printf("then they are read one per line from the specified data file.\n");
   }
   else
   {
      fprintf(stderr,
         "Usage: %s [-r] [-k] [-d] [-f|-i] [-R] [-L|-C|-N|-Q|-A]\n",
         pgm);
      fprintf(stderr,
         "       %*s [-c value] [-h value] [-s string] [-p file]\n",
         (int)strlen(pgm), "");
      fprintf(stderr,
         "       %*s [-e slat wlon nlat elon]\n",
         (int)strlen(pgm), "");
      fprintf(stderr,
         "       %*s filename [lon lat hgt] ...\n",
         (int)strlen(pgm), "");
   }
}

/*------------------------------------------------------------------------
 * process all command-line options
 */
static int process_options(int argc, const char **argv)
{
   int optcnt;

   if ( pgm == GEOCON_NULL )  pgm = strrchr(argv[0], '/');
   if ( pgm == GEOCON_NULL )  pgm = strrchr(argv[0], '\\');
   if ( pgm == GEOCON_NULL )  pgm = argv[0];
   else                       pgm++;

   decimal_pt = localeconv()->decimal_point[0];

   for (optcnt = 1; optcnt < argc; optcnt++)
   {
      const char *arg = argv[optcnt];

      if ( *arg != '-' )
         break;

      while ( *arg == '-' )
         arg++;
      if ( !*arg )
      {
         optcnt++;
         break;
      }

      if ( strcmp(arg, "?")    == 0 ||
           strcmp(arg, "help") == 0 )
      {
         display_usage(1);
         exit(EXIT_SUCCESS);
      }

      else if ( strcmp(arg, "f") == 0 ) direction   = GEOCON_CVT_FORWARD;
      else if ( strcmp(arg, "i") == 0 ) direction   = GEOCON_CVT_INVERSE;
      else if ( strcmp(arg, "k") == 0 ) do_8086     = TRUE;
      else if ( strcmp(arg, "r") == 0 ) reversed    = TRUE;
      else if ( strcmp(arg, "d") == 0 ) read_on_fly = TRUE;
      else if ( strcmp(arg, "R") == 0 ) round_trip  = TRUE;

      else if ( strcmp(arg, "A") == 0 ) interp_all  = TRUE;
      else if ( strcmp(arg, "L") == 0 ) interp      = GEOCON_INTERP_BILINEAR;
      else if ( strcmp(arg, "C") == 0 ) interp      = GEOCON_INTERP_BICUBIC;
      else if ( strcmp(arg, "Q") == 0 ) interp      = GEOCON_INTERP_BIQUADRATIC;
      else if ( strcmp(arg, "N") == 0 ) interp      = GEOCON_INTERP_NATSPLINE;

      else if ( strcmp(arg, "s") == 0 )
      {
         if ( ++optcnt >= argc )
         {
            fprintf(stderr, "%s: option needs an argument -- -%s\n",
               pgm, "s");
            display_usage(0);
            exit(EXIT_FAILURE);
         }
         separator = argv[optcnt];
      }

      else if ( strcmp(arg, "c") == 0 )
      {
         if ( ++optcnt >= argc )
         {
            fprintf(stderr, "%s: option needs an argument -- -%s\n",
               pgm, "c");
            display_usage(0);
            exit(EXIT_FAILURE);
         }
         deg_factor = atof( argv[optcnt] );
      }

      else if ( strcmp(arg, "h") == 0 )
      {
         if ( ++optcnt >= argc )
         {
            fprintf(stderr, "%s: option needs an argument -- -%s\n",
               pgm, "h");
            display_usage(0);
            exit(EXIT_FAILURE);
         }
         hgt_factor = atof( argv[optcnt] );
      }

      else if ( strcmp(arg, "p") == 0 )
      {
         if ( ++optcnt >= argc )
         {
            fprintf(stderr, "%s: option needs an argument -- -%s\n",
               pgm, "p");
            display_usage(0);
            exit(EXIT_FAILURE);
         }
         datafile = argv[optcnt];
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
         fprintf(stderr, "%s: Invalid option -- %s\n", pgm, argv[optcnt]);
         display_usage(0);
         exit(EXIT_FAILURE);
      }
   }

   if ( interp == GEOCON_INTERP_DEFAULT )
      interp = GEOCON_INTERP_BIQUADRATIC;

   if ( do_8086 )
   {
      round_trip = FALSE;
      interp_all = FALSE;
   }

   return optcnt;
}

/*------------------------------------------------------------------------
 * strip a string of all leading/trailing whitespace
 */
static char * strip(char *str)
{
   char * s;
   char * e = GEOCON_NULL;

   for (; isspace(*str); str++) ;

   for (s = str; *s; s++)
   {
      if ( !isspace(*s) )
         e = s;
   }

   if ( e != GEOCON_NULL )
      e[1] = 0;
   else
      *str = 0;

   return str;
}

/*------------------------------------------------------------------------
 * Read *80* and *86* cards
 *
 * "[The original GEOCON program] requires the input file to consist solely of
 * *80* and *86* records, entered pairwise. That is, *80* will always be the
 * odd-numbered records, and *86* records will always be the even-numbered
 * records. And, any given *86* record will be associated with the horizontal
 * position of the *80* record immediately preceding it."
 *
 * This routine emulates that behavior.
 */
static int parse_cols(
   const char * card,
   int          beg,
   int          len)
{
   int num = 0;
   int neg = 0;
   int i;

   for (i = beg; i < beg+len; i++)
   {
      int c = card[i];

      if ( c == '+' )
      {
         continue;
      }
      else if (c == '-' )
      {
         neg = 1;
         continue;
      }

      if ( c == ' ' )
         c = '0';
      num = (num * 10) + (c - '0');
   }

   if ( neg != 0 )
      num = -num;

   return num;
}

static int read_8086(
   FILE   *fp,
   double *plat,
   double *plon,
   double *phgt)
{
   int deg, min;
   int sec_l, sec_r;
   int mtr_l, mtr_r;

   if ( fgets(card_80, sizeof(card_80), fp) == NULL )
      return -1;
   if ( fgets(card_86, sizeof(card_86), fp) == NULL )
      return -1;

   if ( strncmp(card_80+6, "*80*", 4) != 0 )
   {
      printf("invalid *80* card: %s\n", card_80);
      return 1;
   }

   if ( strncmp(card_86+6, "*86*", 4) != 0 )
   {
      printf("invalid *86* card: %s\n", card_86);
      return 1;
   }

   /* latitude */
   {
      deg   = parse_cols(card_80, 44, 2);
      min   = parse_cols(card_80, 46, 2);
      sec_l = parse_cols(card_80, 48, 2);
      sec_r = parse_cols(card_80, 50, 5);

      *plat = (double)deg                     +
              (double)min   / (60           ) +
              (double)sec_l / (3600         ) +
              (double)sec_r / (3600 * 100000) ;

      if ( toupper(card_80[55]) == 'S' )
         *plat = -*plat;
   }

   /* longitude */
   {
      deg   = parse_cols(card_80, 56, 3);
      min   = parse_cols(card_80, 59, 2);
      sec_l = parse_cols(card_80, 61, 2);
      sec_r = parse_cols(card_80, 63, 5);

      *plon = (double)deg                     +
              (double)min   / (60           ) +
              (double)sec_l / (3600         ) +
              (double)sec_r / (3600 * 100000) ;

      if ( toupper(card_80[68]) == 'W' )
         *plon = -*plon;
   }

   /* height */
   {
      mtr_l = parse_cols(card_86, 45, 4);
      mtr_r = parse_cols(card_86, 49, 3);

      if ( mtr_l < 0 )
         mtr_r = -mtr_r;

      *phgt = (double)mtr_l        +
              (double)mtr_r / 1000 ;
   }

   return 0;
}

/*------------------------------------------------------------------------
 * Write *80* and *86* cards
 */
static void store_cols(
   char * card,
   int    num,
   int    beg,
   int    len)
{
   int i;

   for (i = len-1; i >= 0; i--)
   {
      card[beg+i] = (num % 10) + '0';
      num /= 10;
   }
}

static void write_8086(
   double  lat,
   double  lon,
   double  hgt)
{
   int deg, min;
   int sec_l, sec_r;
   int mtr_l, mtr_r;

   /* latitude */
   {
      if ( lat < 0.0 )
      {
         lat = -lat;
         card_80[56] = 'S';
      }
      else
      {
         card_80[56] = 'N';
      }

      deg   = (int)lat; lat = (lat -   deg) * 60;
      min   = (int)lat; lat = (lat -   min) * 60;
      sec_l = (int)lat; lat = (lat - sec_l) * 100000 + .5;
      sec_r = (int)lat;

      store_cols(card_80, deg,   44, 2);
      store_cols(card_80, min,   46, 2);
      store_cols(card_80, sec_l, 48, 2);
      store_cols(card_80, sec_r, 50, 5);
   }

   /* longitude */
   {
      if ( lon < 0.0 )
      {
         lon = -lon;
         card_80[68] = 'W';
      }
      else
      {
         card_80[68] = 'E';
      }

      deg   = (int)lon; lon = (lon -   deg) * 60;
      min   = (int)lon; lon = (lon -   min) * 60;
      sec_l = (int)lon; lon = (lon - sec_l) * 100000 + .5;
      sec_r = (int)lon;

      store_cols(card_80, deg,   56, 3);
      store_cols(card_80, min,   59, 2);
      store_cols(card_80, sec_l, 61, 2);
      store_cols(card_80, sec_r, 63, 5);
   }

   /* height */
   {
      int neg = 0;
      int i;

      if ( hgt < 0.0 )
      {
         neg = 1;
         hgt = -hgt;
      }

      mtr_l = (int)hgt; hgt = (hgt - mtr_l) * 1000 + .5;
      mtr_r = (int)hgt;

      store_cols(card_86, mtr_l, 45, 4);
      store_cols(card_86, mtr_r, 49, 3);

      for (i = 0; i < 3; i++)
      {
         if ( card_86[45+i] != '0' )
            break;
         card_86[45+i] = ' ';
      }
      if ( neg && i > 0 )
         card_86[45+i-1] = '-';
   }

   printf("%s", card_80);
   printf("%s", card_86);
}

/*------------------------------------------------------------------------
 * output a point
 */
static void output_point(
   const char * prefix,
   double       lat,
   double       lon,
   double       hgt)
{
   printf("%s", prefix);

   if ( reversed )
      printf("%.16g%s%.16g%s%.16g\n", lon, separator, lat, separator, hgt);
   else
      printf("%.16g%s%.16g%s%.16g\n", lat, separator, lon, separator, hgt);
   fflush(stdout);
}

/*------------------------------------------------------------------------
 * process a point using a specified interpolation method
 */
static void process_point_interp(
   GEOCON_HDR * hdr,
   int          interp_method,
   const char * interp_name,
   double       lat,
   double       lon,
   double       hgt)
{
   GEOCON_COORD coord[1];
   double       h[1];
   const char * prefix_1st = "--> ";
   const char * prefix_2nd = "<-- ";
   int n;

   if ( direction == GEOCON_CVT_INVERSE )
   {
      const char * t = prefix_1st;
      prefix_1st     = prefix_2nd;
      prefix_2nd     = t;
   }

   coord[0][GEOCON_COORD_LON] = lon;
   coord[0][GEOCON_COORD_LAT] = lat;
   h[0]                       = hgt;

   n = geocon_transform(hdr, interp_method,
      deg_factor, hgt_factor, 1, coord, h, direction);

   if ( round_trip )
   {
      if ( interp_all )
         printf("%-12s: ", interp_name);

      output_point(prefix_1st,
         coord[0][GEOCON_COORD_LAT],
         coord[0][GEOCON_COORD_LON],
         h[0]);
      n = geocon_transform(hdr, interp_method,
         deg_factor, hgt_factor, 1, coord, h, GEOCON_CVT_REVERSE(direction));
   }

   if ( n == 1 )
   {
      lon = coord[0][GEOCON_COORD_LON];
      lat = coord[0][GEOCON_COORD_LAT];
      hgt = h[0];
   }

   if ( interp_all )
      printf("%-12s: ", interp_name);

   if ( do_8086 )
   {
      write_8086(lat, lon, hgt);
   }
   else
   {
      if ( round_trip )
      {
         output_point(prefix_2nd, lat, lon, hgt);
         printf("\n");
      }
      else
      {
         output_point("", lat, lon, hgt);
      }
   }
}

/*------------------------------------------------------------------------
 * process a point
 */
static void process_point(
   GEOCON_HDR * hdr,
   double       lat,
   double       lon,
   double       hgt)
{
   if ( interp_all || interp == GEOCON_INTERP_BILINEAR )
   {
      process_point_interp(
         hdr,
         GEOCON_INTERP_BILINEAR,
         "bilinear",
         lat,
         lon,
         hgt);
   }

   if ( interp_all || interp == GEOCON_INTERP_BICUBIC )
   {
      process_point_interp(
         hdr,
         GEOCON_INTERP_BICUBIC,
         "bicubic",
         lat,
         lon,
         hgt);
   }

   if ( interp_all || interp == GEOCON_INTERP_NATSPLINE )
   {
      process_point_interp(
         hdr,
         GEOCON_INTERP_NATSPLINE,
         "natspline",
         lat,
         lon,
         hgt);
   }

   if ( interp_all || interp == GEOCON_INTERP_BIQUADRATIC )
   {
      process_point_interp(
         hdr,
         GEOCON_INTERP_BIQUADRATIC,
         "biquadratic",
         lat,
         lon,
         hgt);
   }
}

/*------------------------------------------------------------------------
 * process all arguments
 */
static int process_args(
   GEOCON_HDR *   hdr,
   int            optcnt,
   int            argc,
   const char **  argv)
{
   while ( (argc - optcnt) >= 3 )
   {
      double lon, lat, hgt;

      if ( reversed )
      {
        lon = atof( argv[optcnt++] );
        lat = atof( argv[optcnt++] );
        hgt = atof( argv[optcnt++] );
      }
      else
      {
        lat = atof( argv[optcnt++] );
        lon = atof( argv[optcnt++] );
        hgt = atof( argv[optcnt++] );
      }

      process_point(hdr, lat, lon, hgt);
   }

   return 0;
}

/*------------------------------------------------------------------------
 * process a stream of *80* and *86* card pairs
 *
 */
static int process_8086(
   GEOCON_HDR * hdr,
   const char * file)
{
   FILE * fp;

   if ( strcmp(file, "-") == 0 )
   {
      fp = stdin;
   }
   else
   {
      fp = fopen(file, "r");
      if ( fp == GEOCON_NULL )
      {
         fprintf(stderr, "%s: Cannot open data file %s\n", pgm, file);
         return -1;
      }
   }

   for (;;)
   {
      double lat;
      double lon;
      double hgt;
      int rc;

      rc = read_8086(fp, &lat, &lon, &hgt);
      if ( rc < 0 )
         break;
      if ( rc > 0 )
         continue;

      process_point(hdr, lat, lon, hgt);
   }

   fclose(fp);
   return 0;
}

/*------------------------------------------------------------------------
 * process a stream of lon/lat values
 *
 * A line is either:
 *   lat-value lon-value [hgt-value]
 * or (if reversed):
 *   lon-value lat-value [hgt-value]
 *
 * If a line contains only two values, then the height is assumed to be zero.
 *
 * If the decimal point character is not a comma (which it is not in the US),
 * then any commas in the line will be converted to spaces.
 */
static int process_file(
   GEOCON_HDR * hdr,
   const char * file)
{
   FILE * fp;

   if ( strcmp(file, "-") == 0 )
   {
      fp = stdin;
   }
   else
   {
      fp = fopen(file, "r");
      if ( fp == GEOCON_NULL )
      {
         fprintf(stderr, "%s: Cannot open data file %s\n", pgm, file);
         return -1;
      }
   }

   for (;;)
   {
      char   line[128];
      char * lp;
      double lon, lat, hgt;
      int n;

      if ( fgets(line, sizeof(line), fp) == GEOCON_NULL )
         break;

      /* Strip all whitespace to check for an empty line.
         Lines starting with a # are considered comments.
      */
      lp = strip(line);
      if ( *lp == 0 || *lp == '#' )
         continue;

      /* Change any commas to spaces unless the locale's
         decimal point character is a comma.
      */
      if ( decimal_pt != ',' )
      {
         char * s;
         for (s = lp; *s; s++)
         {
            if ( *s == ',' )
               *s = ' ';
         }
      }

      /* Parse lat/lon/hgt or lon/lat/hgt */
      hgt = 0.0;
      if ( reversed )
         n = sscanf(lp, "%lf %lf %lf", &lon, &lat, &hgt);
      else
         n = sscanf(lp, "%lf %lf %lf", &lat, &lon, &hgt);

      /* Must have at least lon & lat (height will default to 0) */
      if ( n < 2 )
      {
         printf("invalid: %s\n", lp);
         continue;
      }

      process_point(hdr, lat, lon, hgt);
   }

   fclose(fp);
   return 0;
}

/*------------------------------------------------------------------------
 * main
 */
int main(int argc, const char **argv)
{
   GEOCON_HDR * hdr;
   int optcnt;
   int gcerr;
   int rc;

   /*---------------------------------------------------------
    * Process command-line options.
    */
   optcnt = process_options(argc, argv);

   /*---------------------------------------------------------
    * Get the filename.
    */
   if ( (argc - optcnt) == 0 )
   {
      fprintf(stderr, "%s: Missing geocon filename\n", pgm);
      display_usage(0);
      return EXIT_FAILURE;
   }
   filename = argv[optcnt++];

   /*---------------------------------------------------------
    * Load the file.
    */
   hdr = geocon_load(
      filename,           /* in:  name             */
      extptr,             /* in:  extent pointer?  */
      !read_on_fly,       /* in:  read shift data? */
      &gcerr);            /* out: result code      */

   if ( hdr == GEOCON_NULL )
   {
      char msg_buf[GEOCON_MAX_ERR_LEN];

      fprintf(stderr, "%s: %s: %s\n",
         pgm, filename, geocon_errmsg(gcerr, msg_buf));
      geocon_delete(hdr);
      return EXIT_FAILURE;
   }

   /*---------------------------------------------------------
    * Either process lon/lat/hgt triples from the cmd line or
    * process all points in the input file.
    */
   if ( optcnt < argc )
   {
      rc = process_args(hdr, optcnt, argc, argv);
   }
   else
   {
      if ( do_8086 )
         rc = process_8086(hdr, datafile);
      else
         rc = process_file(hdr, datafile);
   }

   /*---------------------------------------------------------
    * Done - close out the geocon object and exit.
    */
   geocon_delete(hdr);

   return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
