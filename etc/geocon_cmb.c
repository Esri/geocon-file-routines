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
/*                                                                           */
/* This program will convert old-style single-grid GEOCON binary files       */
/* to a new-style multi-grid GEOCON file, by processing a conversion-file.   */
/*                                                                           */
/* The format of this conversion file is:                                    */
/*                                                                           */
/*    Information                            (max of 79 characters)          */
/*    Source                                 (max of 79 characters)          */
/*    Date                                   (YYYY-MM-DD)                    */
/*    From GCS name                          (max of 79 characters)          */
/*    To   GCS name                          (max of 79 characters)          */
/*    Output path of created   asc/bin file  (*.gcb or *.gca)                */
/*    Input  path of latitude  binary  file  (g*la*.b)                       */
/*    Input  path of longitude binary  file  (g*lo*.b)                       */
/*    Input  path of height    binary  file  (g*v*.b)                        */
/*                                                                           */
/* Note that this program can be used for both the error files and the       */
/* shift files.                                                              */
/*                                                                           */
/* Note also that this program does not deal with reading a binary file      */
/* that is in non-native-endian format. In other words, on a PC it is        */
/* assumed that the input binary files will be in little-endian format.      */
/* The program, however, can write the output file in any endian format      */
/* desired.                                                                  */
/*                                                                           */
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
static int             endian    = GEOCON_ENDIAN_INP_FILE; /* -B | -L | -N */

/*------------------------------------------------------------------------
 * file header for original single-grid files
 */
typedef struct gchdr GCHDR;
struct gchdr
{
   double glamn;    /* latitude  minimum (degrees 0-360)    */
   double glomn;    /* longitude minimum (degrees 0-360)    */
   double dgla;     /* latitude  delta   (degrees)          */
   double dglo;     /* longitude delta   (degrees)          */
   int    nla;      /* num of lat values (num rows of data) */
   int    nlo;      /* num of lon values (num cols of data) */
   int    kind;     /* always 1 (data values are floats)    */
};

/* can't use sizeof(GCHDR) because of padding */
#define GCHDR_LEN  ( (4 * sizeof(double)) + (3 * sizeof(int)) )

/*------------------------------------------------------------------------
 * conversion info struct
 */
typedef struct cvt_info CVT_INFO;
struct cvt_info
{
   char info      [GEOCON_HDR_INFO_LEN];    /* File info                     */
   char source    [GEOCON_HDR_INFO_LEN];    /* Source of info                */
   char date      [GEOCON_HDR_DATE_LEN];    /* Date "YYYY-MM-DD"             */
   char from_gcs  [GEOCON_HDR_NAME_LEN];    /* From GCS name                 */
   char to_gcs    [GEOCON_HDR_NAME_LEN];    /* To   GCS name                 */

   char out_file  [GEOCON_MAX_PATH_LEN];    /* Name of output GEOCON    file */

   char lat_file  [GEOCON_MAX_PATH_LEN];    /* Name of input  latitude  file */
   char lon_file  [GEOCON_MAX_PATH_LEN];    /* Name of input  longitude file */
   char hgt_file  [GEOCON_MAX_PATH_LEN];    /* Name of input  height    file */
};

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

      printf("  -B         Write    big-endian binary file\n");
      printf("  -L         Write little-endian binary file\n");
      printf("  -N         Write native-endian binary file\n");
      printf("               (default is same as input file)\n");
   }
   else
   {
      fprintf(stderr, "Usage: %s [-B|-L|-N] file ...\n", pgm);
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

      else if ( strcmp(arg, "B") == 0 ) endian     = GEOCON_ENDIAN_BIG;
      else if ( strcmp(arg, "L") == 0 ) endian     = GEOCON_ENDIAN_LITTLE;
      else if ( strcmp(arg, "N") == 0 ) endian     = GEOCON_ENDIAN_NATIVE;

      else
      {
         fprintf(stderr, "Invalid option -- %s\n", argv[optcnt]);
         display_usage(0);
         exit(EXIT_FAILURE);
      }
   }

   return optcnt;
}

/*------------------------------------------------------------------------
 * strip a string of all leading/trailing whitespace
 */
static char * strip(char *str)
{
   char * s;
   char * e = NULL;

   for (; isspace(*str); str++) ;

   for (s = str; *s; s++)
   {
      if ( !isspace(*s) )
         e = s;
   }

   if ( e != NULL )
      e[1] = 0;
   else
      *str = 0;

   return str;
}

static char * strip_buf(char *str)
{
   char * s = strip(str);

   if ( s > str )
      strcpy(str, s);
   return str;
}

/*------------------------------------------------------------------------
 * load conversion info from the conversion file
 */
static int get_cvt_info(const char *cvtfile, CVT_INFO *ci)
{
   FILE * fp = fopen(cvtfile, "r");
   char buf[1024];
   int  len;
   int  size;
   int  rc = 0;

   if ( fp == GEOCON_NULL )
   {
      printf("%s: Cannot open conversion file\n", cvtfile);
      return -1;
   }

#define RD_CI(f) \
   fgets(buf, sizeof(buf), fp);                                          \
   if ( feof(fp) )                                                       \
   {                                                                     \
      printf("%s: Unexpected EOF reading field \"%s\"\n", cvtfile, #f);  \
      rc = -1;                                                           \
   }                                                                     \
   strip_buf(buf);                                                       \
   size = (int)sizeof(ci->f);                                            \
   len  = (int)strlen(buf);                                              \
   if ( len >= size )                                                    \
   {                                                                     \
      printf("%s: Field \"%s\" too long: %d, max is %d\n",               \
         cvtfile, #f, len, size);                                        \
      rc = -1;                                                           \
   }                                                                     \
   else                                                                  \
   {                                                                     \
      strcpy(ci->f, buf);                                                \
   }

   RD_CI( info     );
   RD_CI( source   );
   RD_CI( date     );
   RD_CI( from_gcs );
   RD_CI( to_gcs   );
   RD_CI( out_file );
   RD_CI( lat_file );
   RD_CI( lon_file );
   RD_CI( hgt_file );
   fclose(fp);

#undef RD_CI

   return rc;
}

/*------------------------------------------------------------------------
 * read header of original files
 */
static int read_original_hdr(
   FILE       * fp,
   GCHDR      * hdr,
   const char * filename)
{
   int prefix;
   int suffix;
   size_t nr;

   nr = fread(&prefix, sizeof(prefix), 1, fp);
   if ( nr != 1 )
   {
      printf("%s: error reading header prefix\n", filename);
   }
   if ( prefix != (int)GCHDR_LEN )
   {
      printf("%s: invalid header prefix value: %d\n", filename, prefix);
      return -1;
   }

   nr = fread(hdr, GCHDR_LEN, 1, fp);
   if ( nr != 1 )
   {
      printf("%s: error reading header\n", filename);
   }

   nr = fread(&suffix, sizeof(suffix), 1, fp);
   if ( nr != 1 )
   {
      printf("%s: error reading header suffix\n", filename);
   }
   if ( suffix != (int)GCHDR_LEN )
   {
      printf("%s: invalid header suffix value: %d\n", filename, suffix);
      return -1;
   }

   return 0;
}

/*------------------------------------------------------------------------
 * load old files into new format object
 */
static GEOCON_HDR * load_cvt_fp(
   CVT_INFO *ci,
   FILE *    fp_lat,
   FILE *    fp_lon,
   FILE *    fp_hgt)
{
   GEOCON_HDR *  hdr;
   GEOCON_FILE_HDR * fhdr;
   GCHDR         hdr_lat;
   GCHDR         hdr_lon;
   GCHDR         hdr_hgt;
   GEOCON_POINT *pt;
   size_t nr;
   int c;
   int r;
   int rc = 0;

   /* read in headers from all three files */

   rc |= read_original_hdr(fp_lat, &hdr_lat, ci->lat_file);
   rc |= read_original_hdr(fp_lon, &hdr_lon, ci->lon_file);
   rc |= read_original_hdr(fp_hgt, &hdr_hgt, ci->hgt_file);
   if ( rc != 0 )
   {
      return GEOCON_NULL;
   }

   /* make sure all three headers are identical */

   if ( memcmp(&hdr_lat, &hdr_lon, GCHDR_LEN) != 0 )
   {
      printf("lat/lon headers do not match\n");
      return GEOCON_NULL;
   }

   if ( memcmp(&hdr_lat, &hdr_hgt, GCHDR_LEN) != 0 )
   {
      printf("lat/hgt headers do not match\n");
      return GEOCON_NULL;
   }

   /* allocate the new header and copy all values into it */

   hdr = geocon_create();
   if ( hdr == GEOCON_NULL )
   {
      printf("cannot allocate geocon header\n");
      return GEOCON_NULL;
   }

   fhdr = &hdr->fhdr;

   /* file header */

   strcpy(fhdr->info,      ci->info  );
   strcpy(fhdr->source,    ci->source);
   strcpy(fhdr->date,      ci->date  );

   fhdr->lat_dir         = GEOCON_LAT_S_TO_N;
   fhdr->lon_dir         = GEOCON_LON_W_TO_E;

   fhdr->ncols           = hdr_lon.nlo;
   fhdr->nrows           = hdr_lon.nla;

   fhdr->lat_south       = hdr_lon.glamn;
   fhdr->lat_north       = hdr_lon.glamn + ((hdr_lon.nla-1) * hdr_lon.dgla);

   fhdr->lon_west        = hdr_lon.glomn;
   if ( fhdr->lon_west > 180.0 )
      fhdr->lon_west -= 360.0;

   fhdr->lon_east        = hdr_lon.glomn + ((hdr_lon.nlo-1) * hdr_lon.dglo);
   if ( fhdr->lon_east > 180.0 )
      fhdr->lon_east -= 360.0;

   fhdr->lat_delta       = hdr_lon.dgla;
   fhdr->lon_delta       = hdr_lon.dglo;

   fhdr->horz_scale      = (60.0 * 60.0) * 100000.0; /* 0.00001 arc-seconds */
   fhdr->vert_scale      = 100.0;                    /* centimeters         */

   strcpy(fhdr->from_gcs,  ci->from_gcs);
   strcpy(fhdr->from_vcs,  "NAD_1983");              /* NAD 1983 vcs   */
   fhdr->from_semi_major = 6378137.0;                /* GRS80 spheroid */
   fhdr->from_flattening = 298.257222101;

   strcpy(fhdr->to_gcs,    ci->to_gcs);
   strcpy(fhdr->to_vcs,    "NAD_1983");              /* NAD 1983 vcs   */
   fhdr->to_semi_major   = 6378137.0;                /* GRS80 spheroid */
   fhdr->to_flattening   = 298.257222101;

   /* internal header */

   strcpy(hdr->pathname,   ci->out_file);
   hdr->filetype         = GEOCON_FILE_TYPE_BIN;

   hdr->lat_dir          = fhdr->lat_dir;
   hdr->lon_dir          = fhdr->lon_dir;

   hdr->nrows            = fhdr->nrows;
   hdr->ncols            = fhdr->ncols;

   hdr->lat_min          = fhdr->lat_south;
   hdr->lat_max          = fhdr->lat_north;
   hdr->lon_min          = fhdr->lon_west;
   hdr->lon_max          = fhdr->lon_east;

   hdr->lat_delta        = fhdr->lat_delta;
   hdr->lon_delta        = fhdr->lon_delta;
   hdr->horz_scale       = fhdr->horz_scale;
   hdr->vert_scale       = fhdr->vert_scale;

   /* allocate the points array */

   hdr->points = (GEOCON_POINT *)
                 malloc(hdr->nrows * hdr->ncols * sizeof(*hdr->points));
   if ( hdr->points == GEOCON_NULL )
   {
      printf("cannot allocate %d x %d geocon points\n", hdr->nrows, hdr->ncols);
      free(hdr);
      return GEOCON_NULL;
   }

   /* read in all points */

   pt = hdr->points;
   for (r = 0; r < hdr->nrows; r++)
   {
      /* skip over record prefix */

      fseek(fp_lat, 4, SEEK_CUR);
      fseek(fp_lon, 4, SEEK_CUR);
      fseek(fp_hgt, 4, SEEK_CUR);

      /* read in all points in this record */

      for (c = 0; c < hdr->ncols; c++)
      {
         nr = fread(&pt->lat_value, sizeof(pt->lat_value), 1, fp_lat);
         nr = fread(&pt->lon_value, sizeof(pt->lon_value), 1, fp_lon);
         nr = fread(&pt->hgt_value, sizeof(pt->hgt_value), 1, fp_hgt);
         pt++;
      }

      /* skip over record suffix */

      fseek(fp_lat, 4, SEEK_CUR);
      fseek(fp_lon, 4, SEEK_CUR);
      fseek(fp_hgt, 4, SEEK_CUR);
   }

   return hdr;
}

static GEOCON_HDR * load_cvt_info(const char *inpfile)
{
   CVT_INFO     ci;
   GEOCON_HDR * hdr;
   FILE * fp_lat;
   FILE * fp_lon;
   FILE * fp_hgt;

   /* read in conversion file info */

   if ( get_cvt_info(inpfile, &ci) != 0 )
   {
      return GEOCON_NULL;
   }

   /* open all three single-grid files */

   fp_lat = fopen(ci.lat_file, "rb");
   if ( fp_lat == NULL )
   {
      printf("%s: Cannot open lat file\n", ci.lat_file);
      return GEOCON_NULL;
   }

   fp_lon = fopen(ci.lon_file, "rb");
   if ( fp_lon == NULL )
   {
      fclose(fp_lat);
      printf("%s: Cannot open lon file\n", ci.lon_file);
      return GEOCON_NULL;
   }

   fp_hgt = fopen(ci.hgt_file, "rb");
   if ( fp_hgt == NULL )
   {
      fclose(fp_lat);
      fclose(fp_lon);
      printf("%s: Cannot open hgt file\n", ci.hgt_file);
      return GEOCON_NULL;
   }

   /* now read in all headers and data from all three files */

   hdr = load_cvt_fp(&ci, fp_lat, fp_lon, fp_hgt);

   fclose(fp_lat);
   fclose(fp_lon);
   fclose(fp_hgt);

   return hdr;
}

/*------------------------------------------------------------------------
 * process a GEOCON/conversion file
 */
static int process_file(const char *inpfile)
{
   GEOCON_HDR * hdr;
   int gcerr;
   int rc = 0;

   printf("%s\n", inpfile);

   hdr = load_cvt_info(inpfile);
   if ( hdr == GEOCON_NULL )
   {
      return -1;
   }

   rc = geocon_write(hdr, hdr->pathname, endian, &gcerr);
   if ( rc != GEOCON_ERR_OK )
   {
      char msg_buf[GEOCON_MAX_ERR_LEN];
      printf("%s: cannot write output file: %s\n",
         hdr->pathname, geocon_errmsg(gcerr, msg_buf));
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
