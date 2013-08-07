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

#if _WIN32
#  pragma warning (disable: 4996) /* _CRT_SECURE_NO_WARNINGS */
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <locale.h>

#include "libgeocon.h"
#include "libgeocon.i"

#define GEOCON_UNUSED_PARAMETER(p) (void)(p)

/* ------------------------------------------------------------------------- */
/* floating-point comparison macros                                          */
/* ------------------------------------------------------------------------- */

#define GEOCON_EPS_48          3.55271367880050092935562e-15 /* 2^(-48) */
#define GEOCON_EPS_49          1.77635683940025046467781e-15 /* 2^(-49) */
#define GEOCON_EPS_50          8.88178419700125232338905e-16 /* 2^(-50) */
#define GEOCON_EPS_51          4.44089209850062616169453e-16 /* 2^(-51) */
#define GEOCON_EPS_52          2.22044604925031308084726e-16 /* 2^(-52) */
#define GEOCON_EPS_53          1.11022302462515654042363e-16 /* 2^(-53) */

#define GEOCON_EPS             GEOCON_EPS_51 /* best compromise between */
                                             /* speed and accuracy      */

#define GEOCON_ABS(a)          ( ((a) < 0) ? -(a) : (a) )

#define GEOCON_EQ_EPS(a,b,e)   ( ((a) == (b)) || GEOCON_ABS((a)-(b)) <= \
                                 (e)*(1+(GEOCON_ABS(a)+GEOCON_ABS(b))/2) )
#define GEOCON_EQ(a,b)         ( GEOCON_EQ_EPS(a, b, GEOCON_EPS) )

#define GEOCON_NE_EPS(a,b,e)   ( !GEOCON_EQ_EPS(a, b, e) )
#define GEOCON_NE(a,b)         ( !GEOCON_EQ    (a, b   ) )

#define GEOCON_LE_EPS(a,b,e)   ( ((a) < (b))  || GEOCON_EQ_EPS(a,b,e) )
#define GEOCON_LE(a,b)         ( GEOCON_LE_EPS(a, b, GEOCON_EPS) )

#define GEOCON_LT_EPS(a,b,e)   ( !GEOCON_GE_EPS(a, b, e) )
#define GEOCON_LT(a,b)         ( !GEOCON_GE    (a, b   ) )

#define GEOCON_GE_EPS(a,b,e)   ( ((a) > (b))  || GEOCON_EQ_EPS(a,b,e) )
#define GEOCON_GE(a,b)         ( GEOCON_GE_EPS(a, b, GEOCON_EPS) )

#define GEOCON_GT_EPS(a,b,e)   ( !GEOCON_LE_EPS(a, b, e) )
#define GEOCON_GT(a,b)         ( !GEOCON_LE    (a, b   ) )

#define GEOCON_ZERO_EPS(a,e)   ( ((a) == 0.0) || GEOCON_ABS(a) <= (e) )
#define GEOCON_ZERO(a)         ( GEOCON_ZERO_EPS(a, GEOCON_EPS) )

#define GEOCON_MAX(a,b)        ( ((a) > (b)) ? (a) : (b) )
#define GEOCON_MIN(a,b)        ( ((a) < (b)) ? (a) : (b) )

/* ------------------------------------------------------------------------- */
/* GEOCON error messages                                                     */
/* ------------------------------------------------------------------------- */

typedef struct gc_errs GEOCON_ERRS;
struct gc_errs
{
   int         err_num;
   const char *err_msg;
};

static const GEOCON_ERRS gc_errlist[] =
{
   { GEOCON_ERR_OK,                "No error"            },

   { GEOCON_ERR_NO_MEMORY,         "No memory"           },
   { GEOCON_ERR_IOERR,             "I/O error"           },
   { GEOCON_ERR_NULL_PARAMETER,    "NULL parameter"      },

   { GEOCON_ERR_INVALID_EXTENT,    "Invalid extent"      },
   { GEOCON_ERR_FILE_NOT_FOUND,    "File not found"      },
   { GEOCON_ERR_INVALID_FILE,      "Invalid file"        },
   { GEOCON_ERR_CANNOT_OPEN_FILE,  "Cannot open file"    },
   { GEOCON_ERR_UNKNOWN_FILETYPE,  "Unknown filetype"    },
   { GEOCON_ERR_UNEXPECTED_EOF,    "Unexpected EOF"      },
   { GEOCON_ERR_INVALID_TOKEN_CNT, "Invalid token count" },

   { -1, NULL }
};

/* ------------------------------------------------------------------------- */
/* String routines                                                           */
/* ------------------------------------------------------------------------- */

static char * gc_strip(char *str)
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

static char * gc_strip_buf(char *str)
{
   char * s = gc_strip(str);

   if ( s > str )
      strcpy(str, s);
   return str;
}

static int gc_strcmp_i(const char *s1, const char *s2)
{
   for (;;)
   {
      int c1 = toupper(*(const unsigned char *)s1);
      int c2 = toupper(*(const unsigned char *)s2);
      int rc;

      rc = (c1 - c2);
      if ( rc != 0 || c1 == 0 || c2 == 0 )
         return (rc);

      s1++;
      s2++;
   }
}

/* Like strncpy(), but guarantees a null-terminated string
 * and returns number of chars copied.
 */
static int gc_strncpy(char *buf, const char *str, int n)
{
   char * b = buf;
   const char *s;

   for (s = str; --n && *s; s++)
      *b++ = *s;
   *b = 0;

   return (int)(b - buf);
}

/* ------------------------------------------------------------------------- */
/* String tokenizing                                                         */
/* ------------------------------------------------------------------------- */

#define GEOCON_TOKENS_MAX     64
#define GEOCON_TOKENS_BUFLEN  256

typedef struct gc_token GEOCON_TOKEN;
struct gc_token
{
   char   buf  [GEOCON_TOKENS_BUFLEN];
   char * toks [GEOCON_TOKENS_MAX];
   int    num;
};

/* tokenize a buffer
 *
 * This routine splits a line into "tokens", based on the delimiter
 * string.  Each token will have all leading/trailing whitespace
 * and embedding quotes removed from it.
 *
 * If the delimiter string is NULL or empty, then tokenizing will just
 * depend on whitespace.  In this case, multiple whitespace chars will
 * count as a single delimiter.
 *
 * Up to "maxtoks" tokens will be processed.  Any left-over chars will
 * be left in the last token. If there are less tokens than requested,
 * then the remaining token entries will point to an empty string.
 *
 * Return value is the number of tokens found.
 *
 * Note that this routine does not yet support "escaped" chars (\x).
 */
static int gc_str_tokenize(
   GEOCON_TOKEN *ptoks,
   const char *line,
   const char *delims,
   int         maxtoks)
{
   char * p;
   int ntoks = 1;
   int i;

   /* sanity checks */

   if ( ptoks == GEOCON_NULL )
      return 0;

   if ( maxtoks <= 0 || ntoks > GEOCON_TOKENS_MAX )
      maxtoks = GEOCON_TOKENS_MAX;

   if ( line   == GEOCON_NULL )  line   = "";
   if ( delims == GEOCON_NULL )  delims = "";

   /* copy the line, removing any leading/trailing whitespace */

   gc_strncpy(ptoks->buf, line, sizeof(ptoks->buf));
   gc_strip_buf(ptoks->buf);
   ptoks->num = 0;

   if ( *ptoks->buf == 0 )
      return 0;

   /* now do the tokenizing */

   p = ptoks->buf;
   ptoks->toks[0] = p;

   while (ntoks < maxtoks)
   {
      char * s;
      GEOCON_BOOL quotes = FALSE;

      for (s = p; *s; s++)
      {
         if ( quotes )
         {
            if ( *s == '"' )
               quotes = FALSE;
            continue;
         }
         else if ( *s == '"' )
         {
            quotes = TRUE;
            continue;
         }

         if ( *delims == 0 )
         {
            if ( !quotes && isspace(*s) )
               break;
         }
         else
         {
            if ( !quotes && *s == *delims )
               break;
         }
      }
      if ( *s == 0 )
         break;

      *s++ = 0;
      gc_strip(ptoks->toks[ntoks - 1]);

      for (p = s; isspace(*p); p++) ;
      ptoks->toks[ntoks++] = p;
   }

   /* now strip any enbedding quotes from each token */

   for (i = 0; i < ntoks; i++)
   {
      char * str = ptoks->toks[i];
      int len    = (int)strlen(str);
      int c      = *str;

      if ( (c == '\'' || c == '"') && str[len-1] == c )
      {
         str[len-1] = 0;
         ptoks->toks[i] = ++str;
         gc_strip_buf(str);
      }
   }

   /* set rest of requested tokens to empty string */
   for (i = ntoks; i < maxtoks; i++)
      ptoks->toks[i] = "";

   ptoks->num = ntoks;
   return ntoks;
}

/* ------------------------------------------------------------------------- */
/* Byte swapping routines                                                    */
/* ------------------------------------------------------------------------- */

static GEOCON_BOOL gc_is_big_endian(void)
{
   int one = 1;

   return ( *((char *)&one) == 0 );
}

static GEOCON_BOOL gc_is_ltl_endian(void)
{
   return ! gc_is_big_endian();
}

#define SWAP4(a) \
   ( (((a) & 0x000000ff) << 24) | \
     (((a) & 0x0000ff00) <<  8) | \
     (((a) & 0x00ff0000) >>  8) | \
     (((a) & 0xff000000) >> 24) )

static void gc_swap_int(int in[], int ntimes)
{
   int i;

   for (i = 0; i < ntimes; i++)
      in[i] = SWAP4((unsigned int)in[i]);
}

static void gc_swap_flt(float in[], int ntimes)
{
  gc_swap_int((int *)in, ntimes);
}

static void gc_swap_dbl(double in[], int ntimes)
{
   int  i;
   int *p_int, tmpint;

   for (i = 0; i < ntimes; i++)
   {
      p_int = (int *)(&in[i]);
      gc_swap_int(p_int, 2);

      tmpint   = p_int[0];
      p_int[0] = p_int[1];
      p_int[1] = tmpint;
   }
}

/*------------------------------------------------------------------------
 * convert a string to a double
 *
 * Note: Any '.' in the string is first converted to the localized value.
 *       This means the string cannot be const, as it may be modified.
 */
static double gc_atod(char *s)
{
   char   dec_pnt = localeconv()->decimal_point[0];  /* never cache this */
   double d = 0.0;

   if ( s != NULL && *s != 0 )
   {
      char *p = strchr(s, '.');
      if ( p != NULL )
         *p = dec_pnt;
      d = atof(s);
   }

   return d;
}

/*------------------------------------------------------------------------
 * format a floating point number to a string
 *
 * Numbers are displayed left-adjusted with a max of 9 decimal digits,
 * with extra trailing zeros removed.
 *
 * Also, any localized decimal point character is converted to a '.'.
 */
static char * gc_dtoa(char *buf, double dbl)
{
   char dec_pnt = localeconv()->decimal_point[0];  /* never cache this */
   char *s;

   sprintf(buf, "%.9f", dbl);
   s = strchr(buf, dec_pnt);
   if ( s != NULL )
   {
      *s = '.';

#if 0
      /* remove any trailing zeros */
      {
         char *d;
         char *p = NULL;

         s += 2;
         for (d = s; *d; d++)
         {
            if ( *d != '0' )
               p = d;
         }
         if ( p == NULL )
            *s = 0;
         else
            p[1] = 0;
      }
#endif
   }

   return buf;
}

/* -------------------------------------------------------------------------- */
/* internal misc GEOCON routines                                              */
/* -------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * byte-swap a file header
 */
static void gc_flip_hdr(
   GEOCON_FILE_HDR *fhdr)
{
   gc_swap_int( &fhdr->magic,           1 );
   gc_swap_int( &fhdr->version,         1 );
   gc_swap_int( &fhdr->hdrlen,          1 );
   gc_swap_int( &fhdr->reserved,        1 );

   gc_swap_int( &fhdr->lat_dir,         1 );
   gc_swap_int( &fhdr->lon_dir,         1 );

   gc_swap_int( &fhdr->nrows,           1 );
   gc_swap_int( &fhdr->ncols,           1 );

   gc_swap_dbl( &fhdr->lat_south,       1 );
   gc_swap_dbl( &fhdr->lat_north,       1 );

   gc_swap_dbl( &fhdr->lon_west,        1 );
   gc_swap_dbl( &fhdr->lon_east,        1 );

   gc_swap_dbl( &fhdr->lat_delta,       1 );
   gc_swap_dbl( &fhdr->lon_delta,       1 );

   gc_swap_dbl( &fhdr->horz_scale,      1 );
   gc_swap_dbl( &fhdr->vert_scale,      1 );

   gc_swap_dbl( &fhdr->from_semi_major, 1 );
   gc_swap_dbl( &fhdr->from_flattening, 1 );

   gc_swap_dbl( &fhdr->to_semi_major,   1 );
   gc_swap_dbl( &fhdr->to_flattening,   1 );
}

/*------------------------------------------------------------------------
 * byte-swap a point
 */
static void gc_flip_point(
   GEOCON_POINT * pt)
{
   gc_swap_flt( &pt->lat_value, 1 );
   gc_swap_flt( &pt->lon_value, 1 );
   gc_swap_flt( &pt->hgt_value, 1 );
}

/*------------------------------------------------------------------------
 * check if an extent is empty
 */
static GEOCON_BOOL gc_extent_is_empty(const GEOCON_EXTENT *extent)
{
   if ( extent == GEOCON_NULL )
      return TRUE;

   if ( GEOCON_EQ(extent->wlon, extent->elon) ||
        GEOCON_EQ(extent->slat, extent->nlat) )
   {
      return TRUE;
   }

   return FALSE;
}

/*------------------------------------------------------------------------
 * adjust longitude to range (-180, 180)
 */
static double gc_delta(double d)
{
   double a;

   if ( d < -180.0 )  d += 360.0;
   else
   if ( d >  180.0 )  d -= 360.0;

   a = GEOCON_ABS(d);
   if ( !GEOCON_EQ(a, 180.0) )
   {
      d = fmod(d, 180.0);
      d = (GEOCON_ABS(d) <= 180.0) ? d : ((d < 0) ? d+180.0 : d-180.0);
   }

   return d;
}

/* -------------------------------------------------------------------------- */
/* internal read routines                                                     */
/* -------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Read in a line from an ascii stream.
 *
 * This will read in a line, strip all leading and trailing whitespace,
 * and discard any blank lines and comments (anything following a #).
 *
 * Returns NULL at EOF.
 */
static char * gc_read_line(GEOCON_HDR *hdr, char *buf, size_t buflen)
{
   char * bufp;

   for (;;)
   {
      char *p;

      bufp = fgets(buf, (int)buflen, hdr->fp);
      if ( bufp == NULL )
         break;
      hdr->line_count++;

      p = strchr(bufp, '#');
      if ( p != NULL )
         *p = 0;

      bufp = gc_strip(bufp);
      if ( *bufp != 0 )
         break;
   }

   return bufp;
}

/*------------------------------------------------------------------------
 * read in a tokenized line
 *
 * Returns number of tokens or -1 at EOF
 */
static int gc_read_toks(GEOCON_HDR *hdr, GEOCON_TOKEN *ptok, int maxtoks)
{
   char  buf[GEOCON_TOKENS_BUFLEN];
   char *bufp;

   bufp = gc_read_line(hdr, buf, sizeof(buf));
   if ( bufp == NULL )
      return -1;

   return gc_str_tokenize(ptok, bufp, NULL, maxtoks);
}

#define RT(n)    if ( gc_read_toks(hdr, &tok, n) <= 0 )  \
                 {                                       \
                    *prc = GEOCON_ERR_UNEXPECTED_EOF;    \
                    return -1;                           \
                 }                                       \
                 if ( tok.num != n )                     \
                 {                                       \
                    *prc = GEOCON_ERR_INVALID_TOKEN_CNT; \
                    return -1;                           \
                 }

#define TOK(i)   tok.toks[i]

/*------------------------------------------------------------------------
 * load a binary header
 */
static int gc_load_hdr_bin(
   GEOCON_HDR *hdr,
   int        *prc)
{
   GEOCON_FILE_HDR * fhdr = &hdr->fhdr;
   size_t nr;

   nr = fread(fhdr, sizeof(*fhdr), 1, hdr->fp);
   if ( nr != 1 )
   {
      *prc = GEOCON_ERR_IOERR;
      return -1;
   }
   hdr->points_start = ftell(hdr->fp);

   if ( fhdr->magic == GEOCON_HDR_MAGIC_SWAPPED )
   {
      gc_flip_hdr(fhdr);
      hdr->flip = TRUE;
   }

   if ( fhdr->magic != GEOCON_HDR_MAGIC )
   {
      *prc = GEOCON_ERR_INVALID_FILE;
      return -1;
   }

   return 0;
}

/*------------------------------------------------------------------------
 * load an ascii header
 */
static int gc_load_hdr_asc(
   GEOCON_HDR *hdr,
   int        *prc)
{
   GEOCON_FILE_HDR * fhdr = &hdr->fhdr;
   GEOCON_TOKEN tok;

   RT(2); gc_strncpy( fhdr->info,           TOK(1), sizeof(fhdr->info)   );
   RT(2); gc_strncpy( fhdr->source,         TOK(1), sizeof(fhdr->source) );
   RT(2); gc_strncpy( fhdr->date,           TOK(1), sizeof(fhdr->date)   );

   RT(2); fhdr->lat_dir      = (gc_strcmp_i(TOK(1), "N-S") == 0) ?
                                                    GEOCON_LAT_N_TO_S :
                                                    GEOCON_LAT_S_TO_N ;

   RT(2); fhdr->lon_dir      = (gc_strcmp_i(TOK(1), "E-W") == 0) ?
                                                    GEOCON_LON_E_TO_W :
                                                    GEOCON_LON_W_TO_E ;

   RT(2); fhdr->nrows            = atoi(    TOK(1) );
   RT(2); fhdr->ncols            = atoi(    TOK(1) );

   RT(2); fhdr->lat_south        = gc_atod( TOK(1) );
   RT(2); fhdr->lat_north        = gc_atod( TOK(1) );

   RT(2); fhdr->lon_west         = gc_atod( TOK(1) );
   RT(2); fhdr->lon_east         = gc_atod( TOK(1) );

   RT(2); fhdr->lat_delta        = gc_atod( TOK(1) );
   RT(2); fhdr->lon_delta        = gc_atod( TOK(1) );

   RT(2); fhdr->horz_scale       = gc_atod( TOK(1) );
   RT(2); fhdr->vert_scale       = gc_atod( TOK(1) );

   RT(2); gc_strncpy( fhdr->from_gcs,       TOK(1), sizeof(fhdr->from_gcs) );
   RT(2); gc_strncpy( fhdr->from_vcs,       TOK(1), sizeof(fhdr->from_vcs) );
   RT(2); fhdr->from_semi_major  = gc_atod( TOK(1) );
   RT(2); fhdr->from_flattening  = gc_atod( TOK(1) );

   RT(2); gc_strncpy( fhdr->to_gcs,         TOK(1), sizeof(fhdr->to_gcs)   );
   RT(2); gc_strncpy( fhdr->to_vcs,         TOK(1), sizeof(fhdr->to_vcs)   );
   RT(2); fhdr->to_semi_major    = gc_atod( TOK(1) );
   RT(2); fhdr->to_flattening    = gc_atod( TOK(1) );

   return 0;
}

/*------------------------------------------------------------------------
 * load a header
 */
static int gc_load_hdr(
   GEOCON_HDR *hdr,
   int        *prc)
{
   int rc;

   if ( hdr->filetype == GEOCON_FILE_TYPE_BIN )
      rc =  gc_load_hdr_bin(hdr, prc);
   else
      rc =  gc_load_hdr_asc(hdr, prc);

   if ( rc == 0 )
   {
      hdr->lat_dir       = hdr->fhdr.lat_dir;
      hdr->lon_dir       = hdr->fhdr.lon_dir;

      hdr->nrows         = hdr->fhdr.nrows;
      hdr->ncols         = hdr->fhdr.ncols;

      hdr->lat_min       = hdr->fhdr.lat_south;
      hdr->lat_max       = hdr->fhdr.lat_north;
      hdr->lon_min       = hdr->fhdr.lon_west;
      hdr->lon_max       = hdr->fhdr.lon_east;

      hdr->lat_delta     = hdr->fhdr.lat_delta;
      hdr->lon_delta     = hdr->fhdr.lon_delta;
      hdr->horz_scale    = hdr->fhdr.horz_scale;
      hdr->vert_scale    = hdr->fhdr.vert_scale;

      hdr->lat_min_ghost = (hdr->lat_min - hdr->lat_delta);
      hdr->lat_max_ghost = (hdr->lat_max + hdr->lat_delta);
      hdr->lon_min_ghost = (hdr->lon_min - hdr->lon_delta);
      hdr->lon_max_ghost = (hdr->lon_max + hdr->lon_delta);
   }

   return rc;
}

/*------------------------------------------------------------------------
 * adjust a header against an extent
 */
static int gc_adjust_extent(
   GEOCON_HDR    *hdr,
   GEOCON_EXTENT *ext,
   int           *skip_south,
   int           *skip_north,
   int           *skip_west,
   int           *skip_east,
   GEOCON_BOOL    adjust_hdr,
   int           *prc)
{
   double lat_min;
   double lat_max;
   double lon_min;
   double lon_max;
   double d;
   int k;

   *skip_south  = 0;
   *skip_north  = 0;
   *skip_west   = 0;
   *skip_east   = 0;

   if ( ext == GEOCON_NULL )
      return 0;

   lat_min = ext->slat;
   lat_max = ext->nlat;
   lon_min = ext->wlon;
   lon_max = ext->elon;

   if ( GEOCON_GE( lat_min, lat_max ) ||
        GEOCON_GE( lon_min, lon_max ) )
   {
      if ( prc != GEOCON_NULL )
         *prc = GEOCON_ERR_INVALID_EXTENT;
      return -1;
   }

   if ( GEOCON_GE( lat_min, hdr->lat_max ) ||
        GEOCON_LE( lat_max, hdr->lat_min ) ||
        GEOCON_GE( lon_min, hdr->lon_max ) ||
        GEOCON_LE( lon_max, hdr->lon_min ) )
   {
      if ( prc != GEOCON_NULL )
         *prc = GEOCON_ERR_INVALID_EXTENT;
      return -1;
   }

   lat_min = GEOCON_MAX( lat_min, hdr->lat_min );
   lat_max = GEOCON_MIN( lat_max, hdr->lat_max );
   lon_min = GEOCON_MAX( lon_min, hdr->lon_min );
   lon_max = GEOCON_MIN( lon_max, hdr->lon_max );

   /* adjust edges of extent to match multiples of the delta */

   if ( GEOCON_GT(lat_min, hdr->lat_min) )
   {
      d = (lat_min - hdr->lat_min) / hdr->lat_delta;
      k = (int)floor(d);

      if ( k > 0 )
      {
         *skip_south = k;
         if ( adjust_hdr )
         {
            hdr->lat_min += (k * hdr->lat_delta);
            hdr->nrows   -= k;
         }
      }
   }

   if ( GEOCON_LT(lat_max, hdr->lat_max) )
   {
      d = (hdr->lat_max - lat_max) / hdr->lat_delta;
      k = (int)floor(d);

      if ( k > 0 )
      {
         *skip_north = k;
         if ( adjust_hdr )
         {
            hdr->lat_max -= (k * hdr->lat_delta);
            hdr->nrows   -= k;
         }
      }
   }

   if ( GEOCON_GT(lon_min, hdr->lon_min) )
   {
      d = (lon_min - hdr->lon_min) / hdr->lon_delta;
      k = (int)floor(d);

      if ( k > 0 )
      {
         *skip_west = k;
         if ( adjust_hdr )
         {
            hdr->lon_min += (k * hdr->lon_delta);
            hdr->ncols   -= k;
         }
      }
   }

   if ( GEOCON_LT(lon_max, hdr->lon_max) )
   {
      d = (hdr->lon_max - lon_max) / hdr->lon_delta;
      k = (int)floor(d);

      if ( k > 0 )
      {
         *skip_east = k;
         if ( adjust_hdr )
         {
            hdr->lon_max -= (k * hdr->lon_delta);
            hdr->ncols   -= k;
         }
      }
   }

   return 0;
}

/*------------------------------------------------------------------------
 * load binary data, processing it against an extent
 */
static int gc_load_data_ext(
   GEOCON_HDR    *hdr,
   GEOCON_EXTENT *ext,
   int           *prc)
{
   GEOCON_FILE_HDR * fhdr = &hdr->fhdr;
   size_t nr;
   long offset;
   int  skip_south = 0;
   int  skip_north = 0;
   int  skip_west  = 0;
   int  skip_east  = 0;
   int  r;
   int  c;
   int  rc = 0;

   /* Calculate the amount of data to cut out
      and update header (not file header) values.
   */
   rc = gc_adjust_extent(hdr, ext,
      &skip_south, &skip_north, &skip_west, &skip_east, TRUE, prc);
   if ( rc != 0 )
      return rc;

   /* allocate the points array */

   hdr->points = (GEOCON_POINT *)
                 gc_memalloc(hdr->nrows * hdr->ncols * sizeof(*hdr->points));
   if ( hdr->points == GEOCON_NULL )
   {
      *prc = GEOCON_ERR_NO_MEMORY;
      return -1;
   }

   /* skip over any rows at the start that are to be cut out. */

   if ( hdr->lat_dir == GEOCON_LAT_S_TO_N )
      offset = skip_south;
   else
      offset = skip_north;

   if ( offset > 0 )
   {
      offset *= (fhdr->ncols * sizeof(GEOCON_POINT));
      fseek(hdr->fp, offset, SEEK_CUR);
   }

   /* Now read in rows of data.  Note that we may not read all
      the way to the end of the file.
   */
   for (r = 0; r < hdr->nrows; r++)
   {
      GEOCON_POINT * p;

      /* skip over leading values in row to be cut out */

      if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
         offset = skip_west;
      else
         offset = skip_east;

      if ( offset > 0 )
      {
         fseek(hdr->fp, offset * sizeof(*p), SEEK_CUR);
      }

      /* read in data values we want */

      for (c = 0; c < hdr->ncols; c++)
      {
         /* locate where the next point is to be read into */

         if ( hdr->lat_dir == GEOCON_LAT_S_TO_N )
            offset  = (r * hdr->ncols);
         else
            offset  = (((hdr->nrows-1) - r) * hdr->ncols);

         if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
            offset += c;
         else
            offset += ((hdr->ncols-1) - c);

         p = hdr->points + offset;

         nr = fread(p, sizeof(*p), 1, hdr->fp);
         if ( nr != 1 )
         {
            *prc = GEOCON_ERR_IOERR;
            rc = -1;
            break;
         }

         if ( hdr->flip )
         {
            gc_flip_point(p);
         }
      }

      /* skip over trailing values in row to be cut out */

      if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
         offset = skip_east;
      else
         offset = skip_west;

      if ( offset > 0 )
      {
         fseek(hdr->fp, offset * sizeof(*p), SEEK_CUR);
      }
   }

   return rc;
}

/*------------------------------------------------------------------------
 * load binary data
 */
static int gc_load_data_bin(
   GEOCON_HDR    *hdr,
   GEOCON_EXTENT *ext,
   int           *prc)
{
   size_t nr;
   int r;
   int c;
   int rc = 0;

   if ( !gc_extent_is_empty(ext) )
   {
      return gc_load_data_ext(hdr, ext, prc);
   }

   /* allocate memory for the point array */

   hdr->points = (GEOCON_POINT *)
                 gc_memalloc(hdr->nrows * hdr->ncols * sizeof(*hdr->points));
   if ( hdr->points == GEOCON_NULL )
   {
      *prc = GEOCON_ERR_NO_MEMORY;
      return -1;
   }

   /* read in all points */

   for (r = 0; r < hdr->nrows; r++)
   {
      for (c = 0; c < hdr->ncols; c++)
      {
         GEOCON_POINT * p = GEOCON_NULL;
         int offset;

         /* locate where the next point is to be read into */

         if ( hdr->lat_dir == GEOCON_LAT_S_TO_N )
            offset  = (r * hdr->ncols);
         else
            offset  = (((hdr->nrows-1) - r) * hdr->ncols);

         if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
            offset += c;
         else
            offset += ((hdr->ncols-1) - c);

         p = hdr->points + offset;

         /* now read into it & byte-swap it if necessary */

         nr = fread(p, sizeof(*p), 1, hdr->fp);
         if ( nr != 1 )
         {
            *prc = GEOCON_ERR_IOERR;
            rc = -1;
            break;
         }

         if ( hdr->flip )
         {
            gc_flip_point(p);
         }
      }
   }

   return rc;
}

/*------------------------------------------------------------------------
 * load ascii data
 */
static int gc_load_data_asc(
   GEOCON_HDR    *hdr,
   GEOCON_EXTENT *ext,
   int           *prc)
{
   GEOCON_TOKEN tok;
   int r;
   int c;
   int rc = 0;

   GEOCON_UNUSED_PARAMETER(ext);

   /* allocate memory for the point array */

   hdr->points = (GEOCON_POINT *)
                 gc_memalloc(hdr->nrows * hdr->ncols * sizeof(*hdr->points));
   if ( hdr->points == GEOCON_NULL )
   {
      *prc = GEOCON_ERR_NO_MEMORY;
      return -1;
   }

   /* read in all points */

   for (r = 0; r < hdr->nrows; r++)
   {
      for (c = 0; c < hdr->ncols; c++)
      {
         GEOCON_POINT * p = GEOCON_NULL;
         int offset;

         /* locate where the next point is to be read into */

         if ( hdr->lat_dir == GEOCON_LAT_S_TO_N )
            offset  = (r * hdr->ncols);
         else
            offset  = (((hdr->nrows-1) - r) * hdr->ncols);

         if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
            offset += c;
         else
            offset += ((hdr->ncols-1) - c);

         p = hdr->points + offset;

         /* now read into it */

         RT(3);
         p->lat_value = (float)gc_atod( TOK(0) );
         p->lon_value = (float)gc_atod( TOK(1) );
         p->hgt_value = (float)gc_atod( TOK(2) );
      }
   }

   return rc;
}

/*------------------------------------------------------------------------
 * load data
 */
static int gc_load_data(
   GEOCON_HDR    *hdr,
   GEOCON_EXTENT *ext,
   int           *prc)
{
   if ( hdr->filetype == GEOCON_FILE_TYPE_BIN )
      return gc_load_data_bin(hdr, ext, prc);
   else
      return gc_load_data_asc(hdr, ext, prc);
}

/* -------------------------------------------------------------------------- */
/* internal write routines                                                    */
/* -------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * write a binary file
 */
static int gc_write_bin(
   const GEOCON_HDR *hdr,
   const char       *pathname,
   int               byte_order,
   int              *prc)
{
   GEOCON_BOOL swap_data;
   FILE *fp;

   swap_data = hdr->flip;
   switch (byte_order)
   {
      case GEOCON_ENDIAN_BIG:    swap_data ^= !gc_is_big_endian(); break;
      case GEOCON_ENDIAN_LITTLE: swap_data ^= !gc_is_ltl_endian(); break;
      case GEOCON_ENDIAN_NATIVE: swap_data  = FALSE;               break;
   }

   fp = fopen(pathname, "wb");
   if ( fp == GEOCON_NULL )
   {
      *prc = GEOCON_ERR_CANNOT_OPEN_FILE;
      return -1;
   }

   /* write file header */
   {
      GEOCON_FILE_HDR fhdr;

      memcpy(&fhdr, &hdr->fhdr, sizeof(fhdr));

      fhdr.lat_dir    = hdr->lat_dir;
      fhdr.lon_dir    = hdr->lon_dir;
      fhdr.nrows      = hdr->nrows;
      fhdr.ncols      = hdr->ncols;
      fhdr.lat_south  = hdr->lat_min;
      fhdr.lat_north  = hdr->lat_max;
      fhdr.lon_west   = hdr->lon_min;
      fhdr.lon_east   = hdr->lon_max;

      if ( swap_data )
      {
         gc_flip_hdr(&fhdr);
      }
      fwrite(&fhdr, sizeof(fhdr), 1, fp);
   }

   /* write data points */
   {
      int r;
      int c;

      for (r = 0; r < hdr->nrows; r++)
      {
         for (c = 0; c < hdr->ncols; c++)
         {
            GEOCON_POINT pt;
            const GEOCON_POINT * p;
            int offset;

            /* get location of next point to write */

            if ( hdr->lat_dir == GEOCON_LAT_S_TO_N )
               offset  = (r * hdr->ncols);
            else
               offset  = (((hdr->nrows-1) - r) * hdr->ncols);

            if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
               offset += c;
            else
               offset += ((hdr->ncols-1) - c);

            p = hdr->points + offset;

            if ( swap_data )
            {
               pt = *p;
               p  = &pt;
               gc_flip_point(&pt);
            }
            fwrite(p, sizeof(*p), 1, fp);
         }
      }
   }

   fclose(fp);
   return 0;
}

/*------------------------------------------------------------------------
 * write an ascii file
 */
static int gc_write_asc(
   const GEOCON_HDR *hdr,
   const char       *pathname,
   int              *prc)
{
   FILE *fp;

   fp = fopen(pathname, "w");
   if ( fp == GEOCON_NULL )
   {
      *prc = GEOCON_ERR_CANNOT_OPEN_FILE;
      return -1;
   }

   /* write file header */
   {
      const GEOCON_FILE_HDR * fhdr = &hdr->fhdr;

      fprintf(fp, "info             \"%s\"\n", fhdr->info            );
      fprintf(fp, "source           \"%s\"\n", fhdr->source          );
      fprintf(fp, "date             \"%s\"\n", fhdr->date            );
      fprintf(fp, "\n");

      fprintf(fp, "lat_dir          %s\n",  hdr->lat_dir == GEOCON_LAT_S_TO_N ?
                                            "S-N" : "N-S"            );
      fprintf(fp, "lon_dir          %s\n",  hdr->lon_dir == GEOCON_LON_E_TO_W ?
                                            "E-W" : "W-E"            );

      fprintf(fp, "nrows            %d\n",      hdr->nrows           );
      fprintf(fp, "ncols            %d\n",      hdr->ncols           );
      fprintf(fp, "\n");

      fprintf(fp, "lat_south        %.17g\n",   hdr->lat_min         );
      fprintf(fp, "lat_north        %.17g\n",   hdr->lat_max         );

      fprintf(fp, "lon_west         %.17g\n",   hdr->lon_min         );
      fprintf(fp, "lon_east         %.17g\n",   hdr->lon_max         );

      fprintf(fp, "lat_delta        %.17g\n",  fhdr->lat_delta       );
      fprintf(fp, "lon_delta        %.17g\n",  fhdr->lon_delta       );
      fprintf(fp, "\n");

      fprintf(fp, "horz_scale       %.17g\n",  fhdr->horz_scale      );
      fprintf(fp, "vert_scale       %.17g\n",  fhdr->vert_scale      );
      fprintf(fp, "\n");

      fprintf(fp, "from_gcs         \"%s\"\n", fhdr->from_gcs        );
      fprintf(fp, "from_vcs         \"%s\"\n", fhdr->from_vcs        );
      fprintf(fp, "from_semi_major  %.17g\n",  fhdr->from_semi_major );
      fprintf(fp, "from_flattening  %.17g\n",  fhdr->from_flattening );
      fprintf(fp, "\n");

      fprintf(fp, "to_gcs           \"%s\"\n", fhdr->to_gcs          );
      fprintf(fp, "to_vcs           \"%s\"\n", fhdr->to_vcs          );
      fprintf(fp, "to_semi_major    %.17g\n",  fhdr->to_semi_major   );
      fprintf(fp, "to_flattening    %.17g\n",  fhdr->to_flattening   );
   }

   /* write data points */
   {
      int r;
      int c;

      for (r = 0; r < hdr->nrows; r++)
      {
         fprintf(fp, "\n");
         for (c = 0; c < hdr->ncols; c++)
         {
            const GEOCON_POINT * p;
            char buf_lat[24];
            char buf_lon[24];
            char buf_hgt[24];
            int offset;

            /* get location of next point to write */

            if ( hdr->lat_dir == GEOCON_LAT_S_TO_N )
               offset  = (r * hdr->ncols);
            else
               offset  = (((hdr->nrows-1) - r) * hdr->ncols);

            if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
               offset += c;
            else
               offset += ((hdr->ncols-1) - c);

            p = hdr->points + offset;

            fprintf(fp, "%16s %16s %16s\n",
               gc_dtoa(buf_lat, p->lat_value),
               gc_dtoa(buf_lon, p->lon_value),
               gc_dtoa(buf_hgt, p->hgt_value) );
         }
      }
   }

   fclose(fp);
   return 0;
}

/* -------------------------------------------------------------------------- */
/* internal transformation routines                                           */
/* -------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Get a lat/lon shift value (either from a file or from memory).
 */
static void gc_get_shift_from_file(
   const GEOCON_HDR * hdr,
   GEOCON_POINT *     pt,
   int                irow,
   int                icol)
{
   size_t nr = 0;

   if ( hdr->fp != GEOCON_NULL )
   {
      long offset;

      /* get the file offset to the point to be read */

      if ( hdr->lat_dir == GEOCON_LAT_S_TO_N )
         offset  = (irow * hdr->ncols);
      else
         offset  = (((hdr->nrows-1) - irow) * hdr->ncols);

      if ( hdr->lon_dir == GEOCON_LON_W_TO_E )
         offset += icol;
      else
         offset += ((hdr->ncols-1) - icol);

      offset = hdr->points_start + (offset * sizeof(*pt));

      /* do the [thread-protected] read */

      gc_mutex_enter(hdr->mutex);
      {
         fseek(hdr->fp, offset, SEEK_SET);
         nr = fread(pt, sizeof(*pt), 1, hdr->fp);
      }
      gc_mutex_leave(hdr->mutex);
   }

   if ( nr != 1 )
   {
      pt->lat_value = 0.0;
      pt->lon_value = 0.0;
      pt->hgt_value = 0.0;
   }
   else
   {
      if ( hdr->flip )
      {
         gc_flip_point(pt);
      }
   }
}

static void gc_get_shift_from_data(
   const GEOCON_HDR * hdr,
   GEOCON_POINT *     pt,
   int                irow,
   int                icol)
{
   int offset = (irow * hdr->ncols) + icol;

   *pt = hdr->points[offset];
}

static void gc_get_shift(
   const GEOCON_HDR * hdr,
   GEOCON_POINT *     pt,
   int                irow,
   int                icol)
{
   if ( irow < 0 || irow >= hdr->nrows ||
        icol < 0 || icol >= hdr->ncols )
   {
      pt->lat_value = 0.0;
      pt->lon_value = 0.0;
      pt->hgt_value = 0.0;
   }
   else
   {
      if ( hdr->points != GEOCON_NULL )
      {
         gc_get_shift_from_data(hdr, pt, irow, icol);
      }
      else
      if ( hdr->fp     != GEOCON_NULL )
      {
         gc_get_shift_from_file(hdr, pt, irow, icol);
      }
      else
      {
         pt->lat_value = 0.0;
         pt->lon_value = 0.0;
         pt->hgt_value = 0.0;
      }
   }
}

/*------------------------------------------------------------------------
 * calculate the shifts for a point using bilinear interpolation
 */
static void gc_calculate_shifts_bilinear(
   const GEOCON_HDR * hdr,
   double       lat_deg,
   double       lon_deg,
   double *     lat_shift,
   double *     lon_shift,
   double *     hgt_shift)
{
   GEOCON_POINT ptA, ptB, ptC, ptD;
   double       x_grid_index, y_grid_index, dx, dy;
   double       h1,  h2,  h3,  h4;
   double       a00, a01, a10, a11;
   int          icol, irow;

   /* get the corner points around our point */
   {
      x_grid_index = (lon_deg - hdr->lon_min) / hdr->lon_delta;
      y_grid_index = (lat_deg - hdr->lat_min) / hdr->lat_delta;

      icol = (x_grid_index < 0.0) ? -1 : (int)x_grid_index;
      irow = (y_grid_index < 0.0) ? -1 : (int)y_grid_index;

      dx   = (x_grid_index - icol);
      dy   = (y_grid_index - irow);

      /* corner points around p are in this order:

                        points    values
                        -----     ---------
                        C   D     h3     h4
                          p           p
         (irow,icol) -> A   B     h1     h2
      */
      gc_get_shift(hdr, &ptA, irow  , icol  );
      gc_get_shift(hdr, &ptB, irow  , icol+1);
      gc_get_shift(hdr, &ptC, irow+1, icol  );
      gc_get_shift(hdr, &ptD, irow+1, icol+1);
   }

   /* Longitude */
   {
      h1 = ptA.lon_value;
      h2 = ptB.lon_value;
      h3 = ptC.lon_value;
      h4 = ptD.lon_value;

      a00 =  h1;
      a10 = (h2 - h1);
      a01 = (h3 - h1);
      a11 = (h1 - h2) - (h3 - h4);

      *lon_shift = a00 + (a10 * dx) + (a01 * dy) + (a11 * dx * dy);
   }

   /* Latitude */
   {
      h1 = ptA.lat_value;
      h2 = ptB.lat_value;
      h3 = ptC.lat_value;
      h4 = ptD.lat_value;

      a00 =  h1;
      a10 = (h2 - h1);
      a01 = (h3 - h1);
      a11 = (h1 - h2) - (h3 - h4);

      *lat_shift = a00 + (a10 * dx) + (a01 * dy) + (a11 * dx * dy);
   }

   /* Height */
   {
      h1 = ptA.hgt_value;
      h2 = ptB.hgt_value;
      h3 = ptC.hgt_value;
      h4 = ptD.hgt_value;

      a00 =  h1;
      a10 = (h2 - h1);
      a01 = (h3 - h1);
      a11 = (h1 - h2) - (h3 - h4);

      *hgt_shift = a00 + (a10 * dx) + (a01 * dy) + (a11 * dx * dy);
   }
}

/*------------------------------------------------------------------------
 * calculate the shifts for a point using bicubic interpolation
 */
static void gc_calculate_shifts_bicubic(
   const GEOCON_HDR * hdr,
   double       lat_deg,
   double       lon_deg,
   double *     lat_shift,
   double *     lon_shift,
   double *     hgt_shift)
{
   GEOCON_POINT pt[4][4];
   double       h[4][4];
   double       c[4];
   double       a0, a1, a2, a3, d0, d2, d3;
   double       x_grid_index, y_grid_index;
   double       dx, dy;
   int          irow, icol, ir;
   int          i, j;

   /* get the corner points around our point */
   {
      x_grid_index = (lon_deg - hdr->lon_min) / hdr->lon_delta;
      y_grid_index = (lat_deg - hdr->lat_min) / hdr->lat_delta;

      icol = (x_grid_index < 0.0) ? -1 : (int)x_grid_index;
      irow = (y_grid_index < 0.0) ? -1 : (int)y_grid_index;

      dx   = (x_grid_index - icol);
      dy   = (y_grid_index - irow);

      /* corner points around p are in this order:

         points            values
         -------------     ---------------------
         M   N   O   P     h30   h31   h32   h33

         I   J   K   L     h20   h21   h22   h23
               p                     p
         E   F   G   H     h10   h11   h12   h13

         A   B   C   D     h00   h01   h02   h03

         (irow,icol) is at F
      */

      irow -= 1;
      icol -= 1;
      for (i = 0; i < 4; i++)
      {
         ir = irow + i;
         for (j = 0; j < 4; j ++)
         {
            gc_get_shift(hdr, &pt[i][j], ir, icol + j);
         }
      }
   }

   /* Longitude */
   {
      for (i = 0; i < 4; i++)
      {
         for (j = 0; j < 4; j ++)
         {
            h[i][j] = pt[i][j].lon_value;
         }
      }

      for (j = 0; j < 4; j ++)
      {
         a0   = h[1][j];
         d0   = h[0][j] - a0;
         d2   = h[2][j] - a0;
         d3   = h[3][j] - a0;
         a1   = d2 - (d0/3.0 + d3/6.0);
         a2   = (d0 + d2)/2.0;
         a3   = (d3 - d0)/6.0 - d2/2.0;
         c[j] = a0 + dy * (a1 + dy * (a2 + dy * a3));
      }

      a0 = c[1];
      d0 = c[0] - a0;
      d2 = c[2] - a0;
      d3 = c[3] - a0;
      a1 = d2 - (d0/3.0 + d3/6.0);
      a2 = (d0 + d2)/2.0;
      a3 = (d3 - d0)/6.0 - d2/2.0;
      *lon_shift = a0 + dx * (a1 + dx * (a2 + dx * a3));
   }

   /* Latitude */
   {
      for (i = 0; i < 4; i++)
      {
         for (j = 0; j < 4; j ++)
         {
            h[i][j] = pt[i][j].lat_value;
         }
      }

      for (j = 0; j < 4; j ++)
      {
         a0   = h[1][j];
         d0   = h[0][j] - a0;
         d2   = h[2][j] - a0;
         d3   = h[3][j] - a0;
         a1   = d2 - (d0/3.0 + d3/6.0);
         a2   = (d0 + d2)/2.0;
         a3   = (d3 - d0)/6.0 - d2/2.0;
         c[j] = a0 + dy * (a1 + dy * (a2 + dy * a3));
      }

      a0 = c[1];
      d0 = c[0] - a0;
      d2 = c[2] - a0;
      d3 = c[3] - a0;
      a1 = d2 - (d0/3.0 + d3/6.0);
      a2 = (d0 + d2)/2.0;
      a3 = (d3 - d0)/6.0 - d2/2.0;
      *lat_shift = a0 + dx * (a1 + dx * (a2 + dx * a3));
   }

   /* Height */
   {
      for (i = 0; i < 4; i++)
      {
         for (j = 0; j < 4; j ++)
         {
            h[i][j] = pt[i][j].hgt_value;
         }
      }

      for (j = 0; j < 4; j ++)
      {
         a0   = h[1][j];
         d0   = h[0][j] - a0;
         d2   = h[2][j] - a0;
         d3   = h[3][j] - a0;
         a1   = d2 - (d0/3.0 + d3/6.0);
         a2   = (d0 + d2)/2.0;
         a3   = (d3 - d0)/6.0 - d2/2.0;
         c[j] = a0 + dy * (a1 + dy * (a2 + dy * a3));
      }

      a0 = c[1];
      d0 = c[0] - a0;
      d2 = c[2] - a0;
      d3 = c[3] - a0;
      a1 = d2 - (d0/3.0 + d3/6.0);
      a2 = (d0 + d2)/2.0;
      a3 = (d3 - d0)/6.0 - d2/2.0;
      *hgt_shift = a0 + dx * (a1 + dx * (a2 + dx * a3));
   }
}

/*------------------------------------------------------------------------
 * calculate the shifts for a point using biquadratic interpolation
 */
static void gc_calculate_shifts_biquadratic(
   const GEOCON_HDR * hdr,
   double       lat_deg,
   double       lon_deg,
   double *     lat_shift,
   double *     lon_shift,
   double *     hgt_shift)
{
   GEOCON_POINT ptA, ptB, ptC, ptD, ptE, ptF, ptG, ptH, ptI;
   float        lft, cen, rgt;
   double       f0, f1, f2;
   double       tmp1, tmp2, tmp3;
   double       x_grid_index, y_grid_index, dx, dy;
   int          icol_lft, icol_cen, icol_rgt;
   int          irow_bot, irow_cen, irow_top;

   /* get all edge values */
   {
      x_grid_index = (lon_deg - hdr->lon_min) / hdr->lon_delta;
      y_grid_index = (lat_deg - hdr->lat_min) / hdr->lat_delta;

      icol_lft = (x_grid_index < 0.0) ? -1 : (int)x_grid_index;
      icol_cen = icol_lft + 1;
      icol_rgt = icol_lft + 2;

      irow_bot = (y_grid_index < 0.0) ? -1 : (int)y_grid_index;
      irow_cen = irow_bot + 1;
      irow_top = irow_bot + 2;
   }

   /* adjust edges against "phantom" cells */
   {
      /* Check right edge */
      while ( icol_rgt > hdr->ncols )
      {
         icol_lft -= 1;
         icol_cen -= 1;
         icol_rgt -= 1;
      }

      /* Check dx and left edge */
      dx = (lon_deg - hdr->lon_delta * icol_lft - hdr->lon_min) /
           hdr->lon_delta;

      if ( dx < 0.5 && icol_lft > 0 )
      {
         icol_lft -= 1;
         icol_cen -= 1;
         icol_rgt -= 1;
         dx       += 1.0;
      }

      /* Check top edge */
      while ( irow_top > hdr->nrows )
      {
         irow_bot -= 1;
         irow_cen -= 1;
         irow_top -= 1;
      }

      /* Check dy and bottom edge */
      dy = (lat_deg - hdr->lat_delta * irow_bot - hdr->lat_min) /
           hdr->lat_delta;

      if ( dy < 0.5 && irow_bot > 0 )
      {
         irow_bot -= 1;
         irow_cen -= 1;
         irow_top -= 1;
         dy       += 1.0;
      }

      tmp1 = 0.5 * (dx - 1.0);
      tmp2 = 0.5 * (dy - 1.0);
   }

   /* get the corner points around our point */
   {
      /* corner points around p are in this order:

                        G   H   I

                        D   E   F
                          p
         (irow,icol) -> A   B   C
      */
      gc_get_shift(hdr, &ptA, irow_bot, icol_lft);
      gc_get_shift(hdr, &ptB, irow_bot, icol_cen);
      gc_get_shift(hdr, &ptC, irow_bot, icol_rgt);
      gc_get_shift(hdr, &ptD, irow_cen, icol_lft);
      gc_get_shift(hdr, &ptE, irow_cen, icol_cen);
      gc_get_shift(hdr, &ptF, irow_cen, icol_rgt);
      gc_get_shift(hdr, &ptG, irow_top, icol_lft);
      gc_get_shift(hdr, &ptH, irow_top, icol_cen);
      gc_get_shift(hdr, &ptI, irow_top, icol_rgt);
   }

   /* Longitude */
   {
      lft  = ptA.lon_value;
      cen  = ptB.lon_value;
      rgt  = ptC.lon_value;
      tmp3 = cen - lft;
      f0   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      lft  = ptD.lon_value;
      cen  = ptE.lon_value;
      rgt  = ptF.lon_value;
      tmp3 = cen - lft;
      f1   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      lft  = ptG.lon_value;
      cen  = ptH.lon_value;
      rgt  = ptI.lon_value;
      tmp3 = cen - lft;
      f2   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      tmp3 = f1 - f0;
      *lon_shift = f0 + dy * (tmp3 + tmp2 * (f2 - f1 - tmp3));
   }

   /* Latitude */
   {
      lft  = ptA.lat_value;
      cen  = ptB.lat_value;
      rgt  = ptC.lat_value;
      tmp3 = cen - lft;
      f0   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      lft  = ptD.lat_value;
      cen  = ptE.lat_value;
      rgt  = ptF.lat_value;
      tmp3 = cen - lft;
      f1   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      lft  = ptG.lat_value;
      cen  = ptH.lat_value;
      rgt  = ptI.lat_value;
      tmp3 = cen - lft;
      f2   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      tmp3 = f1 - f0;
      *lat_shift = f0 + dy * (tmp3 + tmp2 * (f2 - f1 - tmp3));
   }

   /* Height */
   {
      lft  = ptA.hgt_value;
      cen  = ptB.hgt_value;
      rgt  = ptC.hgt_value;
      tmp3 = cen - lft;
      f0   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      lft  = ptD.hgt_value;
      cen  = ptE.hgt_value;
      rgt  = ptF.hgt_value;
      tmp3 = cen - lft;
      f1   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      lft  = ptG.hgt_value;
      cen  = ptH.hgt_value;
      rgt  = ptI.hgt_value;
      tmp3 = cen - lft;
      f2   = lft + dx * (tmp3 + tmp1 * (rgt - cen - tmp3));

      tmp3 = f1 - f0;
      *hgt_shift = f0 + dy * (tmp3 + tmp2 * (f2 - f1 - tmp3));
   }
}

/*------------------------------------------------------------------------
 * calculate the shifts for a point using natural spline interpolation
 */
static void gc_calculate_shifts_natspline(
   const GEOCON_HDR * hdr,
   double       lat_deg,
   double       lon_deg,
   double *     lat_shift,
   double *     lon_shift,
   double *     hgt_shift)
{
   GEOCON_POINT ptA, ptB, ptC, ptD;
   double       x_grid_index, y_grid_index, dx, dy;
   double       a00, a01, a10, a11;
   double       v_a00, v_a01, v_a10, v_a11;
   double       v_dx2;
   double       v_dy2;
   double       v_1_minus_dx;
   double       v_1_minus_dy;
   double       v_1_minus_dx2;
   double       v_1_minus_dy2;
   double       v_3_minus_2_times_dx;
   double       v_3_minus_2_times_dy;
   double       v_3_minus_2_times_1_minus_dx;
   double       v_3_minus_2_times_1_minus_dy;
   int          icol, irow;

   /* get the corner points around our point */
   {
      x_grid_index = (lon_deg - hdr->lon_min) / hdr->lon_delta;
      y_grid_index = (lat_deg - hdr->lat_min) / hdr->lat_delta;

      icol = (x_grid_index < 0.0) ? -1 : (int)x_grid_index;
      irow = (y_grid_index < 0.0) ? -1 : (int)y_grid_index;

      dx   = (x_grid_index - icol);
      dy   = (y_grid_index - irow);

      v_dx2                        = (dx * dx);
      v_dy2                        = (dy * dy);

      v_1_minus_dx                 = (1 - dx);
      v_1_minus_dy                 = (1 - dy);

      v_1_minus_dx2                = (v_1_minus_dx * v_1_minus_dx);
      v_1_minus_dy2                = (v_1_minus_dy * v_1_minus_dy);

      v_3_minus_2_times_dx         = (3 - 2 * dx);
      v_3_minus_2_times_dy         = (3 - 2 * dy);

      v_3_minus_2_times_1_minus_dx = (3 - 2 * v_1_minus_dx);
      v_3_minus_2_times_1_minus_dy = (3 - 2 * v_1_minus_dy);

      v_a00 = v_1_minus_dx2 *
              v_1_minus_dy2 *
              v_3_minus_2_times_1_minus_dx *
              v_3_minus_2_times_1_minus_dy;

      v_a01 = v_dy2 *
              v_1_minus_dx2 *
              v_3_minus_2_times_dy *
              v_3_minus_2_times_1_minus_dx;

      v_a10 = v_dx2 *
              v_1_minus_dy2 *
              v_3_minus_2_times_dx *
              v_3_minus_2_times_1_minus_dy;

      v_a11 = v_dx2 *
              v_dy2 *
              v_3_minus_2_times_dx *
              v_3_minus_2_times_dy;

      /* corner points around p are in this order:

                        points    values
                        -----     ---------
                        C   D     a01   a11
                          p           p
         (irow,icol) -> A   B     a00   a10
      */
      gc_get_shift(hdr, &ptA, irow  , icol  );
      gc_get_shift(hdr, &ptB, irow  , icol+1);
      gc_get_shift(hdr, &ptC, irow+1, icol  );
      gc_get_shift(hdr, &ptD, irow+1, icol+1);
   }

   /* Longitude */
   {
      a00 = ptA.lon_value;
      a10 = ptB.lon_value;
      a01 = ptC.lon_value;
      a11 = ptD.lon_value;

      *lon_shift = a00 * v_a00 +
                   a01 * v_a01 +
                   a10 * v_a10 +
                   a11 * v_a11 ;
   }

   /* Latitude */
   {
      a00 = ptA.lat_value;
      a10 = ptB.lat_value;
      a01 = ptC.lat_value;
      a11 = ptD.lat_value;

      *lat_shift = a00 * v_a00 +
                   a01 * v_a01 +
                   a10 * v_a10 +
                   a11 * v_a11 ;
   }

   /* Height */
   {
      a00 = ptA.hgt_value;
      a10 = ptB.hgt_value;
      a01 = ptC.hgt_value;
      a11 = ptD.hgt_value;

      *hgt_shift = a00 * v_a00 +
                   a01 * v_a01 +
                   a10 * v_a10 +
                   a11 * v_a11 ;
   }
}

/*------------------------------------------------------------------------
 * calculate the shifts for a point
 *
 * The original GEOCON specification only provided for an abrupt edge
 * around the grid, which resulted in the posibility of a point near
 * an edge being shifted to outside the grid, and thus not being able to
 * be inversed back into the grid. Not good.
 *
 * Thus, we pretend that there is a one-cell zone around each grid
 * that has a shift value of zero.  This allows for a point that was
 * shifted out to be properly shifted back, and also allows for
 * a gradual change rather than an abrupt jump.  Much better!
 *
 * Note that this will cause shift values near an edge to be different
 * between this algorithm's results and the original algorithm's results.
 *
 * Note also that we defer applying any conversion factors until after
 * doing any interpolation, in order to preserve accuracy.
 */
static void gc_calculate_shifts(
   const GEOCON_HDR * hdr,
   int          interp,
   double       lat_deg,
   double       lon_deg,
   double *     lat_shift,
   double *     lon_shift,
   double *     hgt_shift)
{
   switch (interp)
   {
      case GEOCON_INTERP_BILINEAR:
         gc_calculate_shifts_bilinear   (hdr, lat_deg,   lon_deg,
                                              lat_shift, lon_shift, hgt_shift);
         break;

      case GEOCON_INTERP_BICUBIC:
         gc_calculate_shifts_bicubic    (hdr, lat_deg,   lon_deg,
                                              lat_shift, lon_shift, hgt_shift);
         break;

      default:
      case GEOCON_INTERP_BIQUADRATIC:
         gc_calculate_shifts_biquadratic(hdr, lat_deg,   lon_deg,
                                              lat_shift, lon_shift, hgt_shift);
         break;

      case GEOCON_INTERP_NATSPLINE:
         gc_calculate_shifts_natspline  (hdr, lat_deg,   lon_deg,
                                              lat_shift, lon_shift, hgt_shift);
         break;
   }

   *lat_shift /= hdr->horz_scale;
   *lon_shift /= hdr->horz_scale;
   *hgt_shift /= hdr->vert_scale;
}

/* -------------------------------------------------------------------------- */
/* external GEOCON routines                                                   */
/* -------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Determine the file type of a name
 *
 * This is done solely by checking the filename extension.
 * No examination of the file contents (if any) is done.
 */
int geocon_filetype(const char *pathname)
{
   const char *ext;

   if ( pathname == GEOCON_NULL || *pathname == 0 )
      return GEOCON_FILE_TYPE_UNK;

   ext = strrchr(pathname, '.');
   if ( ext != GEOCON_NULL )
   {
      ext++;

      if ( gc_strcmp_i(ext, GEOCON_BIN_EXTENSION) == 0 )
         return GEOCON_FILE_TYPE_BIN;

      if ( gc_strcmp_i(ext, GEOCON_ASC_EXTENSION) == 0 )
         return GEOCON_FILE_TYPE_ASC;
   }

   return GEOCON_FILE_TYPE_UNK;
}

/*------------------------------------------------------------------------
 * get a string associated with an error code
 */
const char * geocon_errmsg(int err_num, char msg_buf[])
{
   const GEOCON_ERRS *e;

   for (e = gc_errlist; e->err_num >= 0; e++)
   {
      if ( e->err_num == err_num )
      {
         if ( msg_buf == NULL )
         {
            return e->err_msg;
         }
         else
         {
            strcpy(msg_buf, e->err_msg);
            return msg_buf;
         }
      }
   }

   if ( msg_buf == NULL )
   {
      return "?";
   }
   else
   {
      sprintf(msg_buf, "%d", err_num);
      return msg_buf;
   }
}

/*------------------------------------------------------------------------
 * create an empty GEOCON header
 */
GEOCON_HDR * geocon_create()
{
   GEOCON_HDR * hdr = (GEOCON_HDR *)gc_memalloc(sizeof(*hdr));

   if ( hdr != GEOCON_NULL )
   {
      memset(hdr, 0, sizeof(*hdr));
      hdr->fhdr.magic    = GEOCON_HDR_MAGIC;
      hdr->fhdr.version  = GEOCON_HDR_VERSION;
      hdr->fhdr.hdrlen   = GEOCON_FILE_HDR_LEN;
   }

   return hdr;
}

/*------------------------------------------------------------------------
 * load header and optionally the data
 */
GEOCON_HDR * geocon_load(
   const char    *pathname,
   GEOCON_EXTENT *ext,
   GEOCON_BOOL    load_data,
   int           *prc)
{
   GEOCON_HDR * hdr;
   int filetype;
   int gcerr;
   int rc;

   if ( prc == GEOCON_NULL )
      prc = &gcerr;
   *prc = GEOCON_ERR_OK;

   if ( pathname == GEOCON_NULL || *pathname == 0 )
   {
      *prc = GEOCON_ERR_NULL_PARAMETER;
      return GEOCON_NULL;
   }

   filetype = geocon_filetype(pathname);
   if ( filetype == GEOCON_FILE_TYPE_UNK )
   {
      *prc = GEOCON_ERR_UNKNOWN_FILETYPE;
      return GEOCON_NULL;
   }

   hdr = geocon_create();
   if ( hdr == GEOCON_NULL )
   {
      *prc = GEOCON_ERR_NO_MEMORY;
      return GEOCON_NULL;
   }

   hdr->fp = fopen(pathname, "rb");
   if ( hdr->fp == GEOCON_NULL )
   {
      *prc = GEOCON_ERR_FILE_NOT_FOUND;
      gc_memdealloc(hdr);
      return GEOCON_NULL;
   }

   strcpy(hdr->pathname, pathname);
   hdr->filetype = filetype;

   rc = gc_load_hdr(hdr, prc);
   if ( rc != 0 )
   {
      geocon_delete(hdr);
      return GEOCON_NULL;
   }

   if ( load_data )
   {
      rc = gc_load_data(hdr, ext, prc);

      /* Done with the file whether there were errors or not. */
      fclose(hdr->fp);
      hdr->fp = GEOCON_NULL;

      if ( rc != 0 )
      {
         geocon_delete(hdr);
         return GEOCON_NULL;
      }
   }
   else
   {
      if ( hdr->filetype == GEOCON_FILE_TYPE_BIN )
      {
         hdr->mutex = (void *)gc_mutex_create();
      }
      else
      {
         /* No reading on-the-fly if it's an ascii file. */
         fclose(hdr->fp);
         hdr->fp = GEOCON_NULL;
      }
   }

   return hdr;
}

/*------------------------------------------------------------------------
 * write a geocon file
 */
int geocon_write(
   const GEOCON_HDR *hdr,
   const char       *pathname,
   int               byte_order,
   int              *prc)
{
   int filetype;
   int gcerr;

   if ( prc == GEOCON_NULL )
      prc = &gcerr;
   *prc = GEOCON_ERR_OK;

   if ( hdr == GEOCON_NULL || pathname == GEOCON_NULL || *pathname == 0 )
   {
      *prc = GEOCON_ERR_NULL_PARAMETER;
      return -1;
   }

   filetype = geocon_filetype(pathname);
   if ( filetype == GEOCON_FILE_TYPE_UNK )
   {
      *prc = GEOCON_ERR_UNKNOWN_FILETYPE;
      return -1;
   }

   if ( filetype == GEOCON_FILE_TYPE_BIN )
      return gc_write_bin(hdr, pathname, byte_order, prc);
   else
      return gc_write_asc(hdr, pathname,             prc);
}

/*------------------------------------------------------------------------
 * delete the GEOCON object
 */
void geocon_delete(
   GEOCON_HDR    *hdr)
{
   if ( hdr != GEOCON_NULL )
   {
      if ( hdr->fp != GEOCON_NULL )
         fclose(hdr->fp);

      if ( hdr->mutex != GEOCON_NULL )
         gc_mutex_delete(hdr->mutex);

      if ( hdr->points != GEOCON_NULL )
         gc_memdealloc(hdr->points);

      gc_memdealloc(hdr);
   }
}

/*------------------------------------------------------------------------
 * dump a header in list format
 */
void geocon_list_hdr(
   const GEOCON_HDR *hdr,
   FILE             *fp,
   GEOCON_BOOL       do_hdr_line)
{
   if ( hdr != GEOCON_NULL && fp != GEOCON_NULL )
   {
      const char * path = hdr->pathname;

      if ( do_hdr_line )
      {
         fprintf(fp, "filename              lon-min lat-min "
                     " lon-max lat-max  d-lon  d-lat nrow ncol\n");
         fprintf(fp, "-------------------- -------- ------- "
                     "-------- ------- ------ ------ ---- ----\n");
      }

      if ( strlen(path) > 20 )
      {
         fprintf(fp, "%s\n", path);
         path = "";
      }

      fprintf(fp, "%-20s %8.3f %7.3f %8.3f %7.3f %6.3f %6.3f %4d %4d\n",
         path,
         hdr->lon_min,
         hdr->lat_min,
         hdr->lon_max,
         hdr->lat_max,
         hdr->lon_delta,
         hdr->lat_delta,
         hdr->nrows,
         hdr->ncols);
   }
}

/*------------------------------------------------------------------------
 * dump a header
 */
void geocon_dump_hdr(
   const GEOCON_HDR *hdr,
   FILE             *fp)
{
   if ( hdr != GEOCON_NULL && fp != GEOCON_NULL )
   {
      fprintf(fp, "path              = %s\n",     hdr->pathname             );

      fprintf(fp, "  info            = \"%s\"\n", hdr->fhdr.info            );
      fprintf(fp, "  source          = \"%s\"\n", hdr->fhdr.source          );
      fprintf(fp, "  date            = \"%s\"\n", hdr->fhdr.date            );
      fprintf(fp, "\n");

      fprintf(fp, "  lat_dir         = %s\n",
         hdr->lat_dir == GEOCON_LAT_S_TO_N ? "S-N" : "N-S"                  );
      fprintf(fp, "  lon_dir         = %s\n",
         hdr->lon_dir == GEOCON_LON_E_TO_W ? "E-W" : "W-E"                  );
      fprintf(fp, "\n");

      fprintf(fp, "  nrows           = %4d\n",    hdr->nrows                );
      fprintf(fp, "  ncols           = %4d\n",    hdr->ncols                );
      fprintf(fp, "\n");

      fprintf(fp, "  lat_south       = %.17g\n",  hdr->lat_min              );
      fprintf(fp, "  lat_north       = %.17g\n",  hdr->lat_max              );
      fprintf(fp, "\n");

      fprintf(fp, "  lon_west        = %.17g\n",  hdr->lon_min              );
      fprintf(fp, "  lon_east        = %.17g\n",  hdr->lon_max              );
      fprintf(fp, "\n");

      fprintf(fp, "  lat_delta       = %.17g\n",  hdr->lat_delta            );
      fprintf(fp, "  lon_delta       = %.17g\n",  hdr->lon_delta            );
      fprintf(fp, "\n");

      fprintf(fp, "  horz_scale      = %.17g\n",  hdr->fhdr.horz_scale      );
      fprintf(fp, "  vert_scale      = %.17g\n",  hdr->fhdr.vert_scale      );
      fprintf(fp, "\n");

      fprintf(fp, "  from_gcs        = \"%s\"\n", hdr->fhdr.from_gcs        );
      fprintf(fp, "  from_vcs        = \"%s\"\n", hdr->fhdr.from_vcs        );
      fprintf(fp, "  from_semi_major = %.17g\n",  hdr->fhdr.from_semi_major );
      fprintf(fp, "  from_flattening = %.17g\n",  hdr->fhdr.from_flattening );
      fprintf(fp, "\n");

      fprintf(fp, "  to_gcs          = \"%s\"\n", hdr->fhdr.to_gcs          );
      fprintf(fp, "  to_vcs          = \"%s\"\n", hdr->fhdr.to_vcs          );
      fprintf(fp, "  to_semi_major   = %.17g\n",  hdr->fhdr.to_semi_major   );
      fprintf(fp, "  to_flattening   = %.17g\n",  hdr->fhdr.to_flattening   );
      fprintf(fp, "\n");
   }
}

/*------------------------------------------------------------------------
 * dump all data
 */
void geocon_dump_data(
   const GEOCON_HDR *hdr,
   FILE             *fp)
{
   if ( hdr != GEOCON_NULL && fp != GEOCON_NULL && hdr->points != GEOCON_NULL )
   {
      int c;
      int r;

      for (r = 0; r < hdr->nrows; r++)
      {
         GEOCON_POINT * p = hdr->points + (r * hdr->ncols);
         double lat = hdr->lat_min + (r * hdr->lat_delta);
         double lon = hdr->lon_min;

         fprintf(fp,
            "     lat       lon  "
            "       lat-shift         lon-shift         hgt-shift\n");
         fprintf(fp,
            "--------  --------  "
            "----------------  ----------------  ----------------\n");

         for (c = 0; c < hdr->ncols; c++)
         {
            fprintf(fp, "%8.3f  %8.3f  %16.9f  %16.9f  %16.9f\n",
               lat, lon, p->lat_value, p->lon_value, p->hgt_value);
            lon += hdr->lon_delta;
            p++;
         }

         fprintf(fp, "\n");
      }
   }
}

/*------------------------------------------------------------------------
 * do a forward transformation of points
 */
int geocon_forward(
   const GEOCON_HDR *hdr,
   int               interp,
   double            deg_factor,
   double            hgt_factor,
   int               n,
   GEOCON_COORD      coord[],
   double            h[])
{
   int num = 0;
   int i;

   if ( hdr == GEOCON_NULL || coord == GEOCON_NULL || n <= 0 )
   {
      return 0;
   }

   for (i = 0; i < n; i++)
   {
      double lat_deg, lat_shift;
      double lon_deg, lon_shift;
      double hgt_mtr, hgt_shift;

      lat_deg =          (coord[i][GEOCON_COORD_LAT] * deg_factor);
      lon_deg = gc_delta((coord[i][GEOCON_COORD_LON] * deg_factor));
      hgt_mtr = (h == GEOCON_NULL) ? 0 :       (h[i] * hgt_factor);

      if ( GEOCON_GT(lat_deg, hdr->lat_min_ghost) &&
           GEOCON_LT(lat_deg, hdr->lat_max_ghost) &&
           GEOCON_GT(lon_deg, hdr->lon_min_ghost) &&
           GEOCON_LT(lon_deg, hdr->lon_max_ghost) )
      {
         gc_calculate_shifts(hdr, interp, lat_deg, lon_deg,
            &lat_shift, &lon_shift, &hgt_shift);

         lat_deg += lat_shift;
         lon_deg += lon_shift;
         hgt_mtr += hgt_shift;

         coord[i][GEOCON_COORD_LAT] = (         lat_deg  / deg_factor);
         coord[i][GEOCON_COORD_LON] = (gc_delta(lon_deg) / deg_factor);
         if (h != GEOCON_NULL) h[i] = (         hgt_mtr  / hgt_factor);

         num++;
      }
   }

   return num;
}

/*------------------------------------------------------------------------
 * do an inverse transformation of points
 *
 * Note this routine will usually calculate different values than the
 * original GEOCON code produced, since the original algorithm just
 * subtracted the differences once, rather than iterating down through
 * successive subtractions to get to the result.
 */
#ifndef   MAX_ITERATIONS
#  define MAX_ITERATIONS  50  /* set to 1 to imitate original GEOCON code */
#endif

int geocon_inverse(
   const GEOCON_HDR *hdr,
   int               interp,
   double            deg_factor,
   double            hgt_factor,
   int               n,
   GEOCON_COORD      coord[],
   double            h[])
{
   int max_iterations = MAX_ITERATIONS;
   int num = 0;
   int i;

   if ( hdr == GEOCON_NULL || coord == GEOCON_NULL || n <= 0 )
   {
      return 0;
   }

   for (i = 0; i < n; i++)
   {
      double lat_deg, lat_next;
      double lon_deg, lon_next;
      double hgt_mtr, hgt_next;
      int num_iterations;

      lat_next = lat_deg =          (coord[i][GEOCON_COORD_LAT] * deg_factor);
      lon_next = lon_deg = gc_delta((coord[i][GEOCON_COORD_LON] * deg_factor));
      hgt_next = hgt_mtr = (h == GEOCON_NULL) ? 0       : (h[i] * hgt_factor);

      if ( GEOCON_GT(lat_deg, hdr->lat_min_ghost) &&
           GEOCON_LT(lat_deg, hdr->lat_max_ghost) &&
           GEOCON_GT(lon_deg, hdr->lon_min_ghost) &&
           GEOCON_LT(lon_deg, hdr->lon_max_ghost) )
      {
         /* The inverse is not a simple transformation like the forward.
            We have to iteratively zero in on the answer by successively
            calculating what the forward delta is at the point, and then
            subtracting it instead of adding it.  The assumption here
            is that all the shifts are smooth, which should be the case.

            If we can't get the lat and lon deltas between two steps to be
            both within a given tolerance in max_iterations,
            we just give up and use the last value we calculated.
         */

         for (num_iterations = 0;
              num_iterations < max_iterations;
              num_iterations++)
         {
            double lat_shift, lat_delta, lat_est;
            double lon_shift, lon_delta, lon_est;
            double hgt_shift, hgt_delta, hgt_est;

            gc_calculate_shifts(hdr, interp, lat_next, lon_next,
               &lat_shift, &lon_shift, &hgt_shift);

            lat_est   = (lat_next + lat_shift);
            lon_est   = (lon_next + lon_shift);
            hgt_est   = (hgt_next + hgt_shift);

            lat_delta = (lat_est  - lat_deg);
            lon_delta = (lon_est  - lon_deg);
            hgt_delta = (hgt_est  - hgt_mtr);

#if DEBUG_INVERSE
            {
               fprintf(stderr, "iteration %2d: value: %.17g %.17g %.17g\n",
                  num_iterations+1,
                  lon_next - lon_delta,
                  lat_next - lat_delta,
                  hgt_next - hgt_delta);
               fprintf(stderr, "              delta: %.17g %.17g %.17g\n",
                  lon_delta, lat_delta, hgt_delta);
            }
#endif

            if ( GEOCON_ZERO(lon_delta) &&
                 GEOCON_ZERO(lat_delta) &&
                 GEOCON_ZERO(hgt_delta) )
            {
               break;
            }

            lat_next  = (lat_next - lat_delta);
            lon_next  = (lon_next - lon_delta);
            hgt_next  = (hgt_next - hgt_delta);
         }

#if DEBUG_INVERSE
         {
            fprintf(stderr, "final         value: %.17g %.17g %.17g\n",
               lon_next, lat_next, hgt_next);
         }
#endif

         coord[i][GEOCON_COORD_LAT] = (         lat_next  / deg_factor);
         coord[i][GEOCON_COORD_LON] = (gc_delta(lon_next) / deg_factor);
         if (h != GEOCON_NULL) h[i] = (         hgt_next  / hgt_factor);

         num++;
      }
   }

   return num;
}

/*------------------------------------------------------------------------
 * do a forward/inverse transformation of points
 */
int geocon_transform(
   const GEOCON_HDR *hdr,
   int               interp,
   double            deg_factor,
   double            hgt_factor,
   int               n,
   GEOCON_COORD      coord[],
   double            h[],
   int               direction)
{
   if ( direction == GEOCON_CVT_FORWARD )
      return geocon_forward(hdr, interp, deg_factor, hgt_factor, n, coord, h);
   else
      return geocon_inverse(hdr, interp, deg_factor, hgt_factor, n, coord, h);
}
