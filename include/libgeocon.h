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

/*---------------------------------------------------------------------------*/
/* This is the public header for the GEOCON API, which is a C API providing  */
/* the ability to read GEOCON files, write GEOCON files, and use GEOCON      */
/* files to convert coordinates (both forward and inverse).                  */
/* ------------------------------------------------------------------------- */

#ifndef LIBGEOCON_H_INCLUDED
#define LIBGEOCON_H_INCLUDED

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* version info                                                              */
/* ------------------------------------------------------------------------- */

#define GEOCON_VERSION_MAJOR     1
#define GEOCON_VERSION_MINOR     0
#define GEOCON_VERSION_RELEASE   0
#define GEOCON_VERSION_STR       "1.0.0"

/*---------------------------------------------------------------------------*/
/* external definitions & structs                                            */
/*---------------------------------------------------------------------------*/

#define FALSE                    0
#define TRUE                     1

#define GEOCON_NULL              0             /*!< NULL pointer            */

#define GEOCON_MAX_PATH_LEN    256             /*!< Max pathname length     */
#define GEOCON_MAX_ERR_LEN      32             /*!< Max err msg  length     */

typedef int                GEOCON_BOOL;        /*!< Boolean variable        */
typedef double             GEOCON_COORD [2];   /*!< Lon/lat coordinate      */

#define GEOCON_COORD_LON         0             /*!< Longitude coord index   */
#define GEOCON_COORD_LAT         1             /*!< Latitude  coord index   */

#define GEOCON_COORD_LAM         0             /*!< Longitude coord index   */
#define GEOCON_COORD_PHI         1             /*!< Latitude  coord index   */

/*------------------------------------------------------------------------
 * GEOCON defines
 */
#define GEOCON_HDR_MAGIC          0x47434f4e    /*!< "GCON" (not swapped)   */
#define GEOCON_HDR_MAGIC_SWAPPED  0x4e4f4347    /*!< "NOCG" (    swapped)   */

#define GEOCON_HDR_VERSION        1

#define GEOCON_HDR_INFO_LEN      80   /*!< Max length of info field         */
#define GEOCON_HDR_DATE_LEN      24   /*!< Max length of date field         */
#define GEOCON_HDR_NAME_LEN      80   /*!< Max length of obj  names         */

/* data organization in file */

#define GEOCON_LAT_S_TO_N         0   /*!< Latitude  values go from S to N  */
#define GEOCON_LAT_N_TO_S         1   /*!< Latitude  values go from N to S  */

#define GEOCON_LON_W_TO_E         0   /*!< Longitude values go from W to E  */
#define GEOCON_LON_E_TO_W         1   /*!< Longitude values go from E to W  */

/* filename extensions */

#define GEOCON_BIN_EXTENSION      "gcb"   /*!< Geocon Combined Binary       */
#define GEOCON_ASC_EXTENSION      "gca"   /*!< Geocon Combined Ascii        */

#define GEOCON_FILE_TYPE_UNK      0   /*!< File type is unknown             */
#define GEOCON_FILE_TYPE_BIN      1   /*!< File type is binary              */
#define GEOCON_FILE_TYPE_ASC      2   /*!< File type is ascii               */

/* output byte-order options */

#define GEOCON_ENDIAN_INP_FILE    0   /*!< Write input-file    byte-order   */
#define GEOCON_ENDIAN_BIG         1   /*!< Write big-endian    byte-order   */
#define GEOCON_ENDIAN_LITTLE      2   /*!< Write little-endian byte-order   */
#define GEOCON_ENDIAN_NATIVE      3   /*!< Write native        byte-order   */

/* interpolation types */

#define GEOCON_INTERP_DEFAULT     0   /*!< Use default        interpolation */
#define GEOCON_INTERP_BILINEAR    1   /*!< Use bilinear       interpolation */
#define GEOCON_INTERP_BICUBIC     2   /*!< Use bicubic        interpolation */
#define GEOCON_INTERP_BIQUADRATIC 3   /*!< Use biquadratic    interpolation */
#define GEOCON_INTERP_NATSPLINE   4   /*!< Use natural spline interpolation */

/* transformation directions */

#define GEOCON_CVT_FORWARD        1   /*!< Convert data forward             */
#define GEOCON_CVT_INVERSE        0   /*!< Convert data inverse             */
#define GEOCON_CVT_REVERSE(n)     (1 - n)   /*!< Reverse the direction      */

/*---------------------------------------------------------------------------*/
/**
 * GEOCON file header
 *
 * This is an image of the header part of a binary GEOCON file.
 * Note that all character-string fields should be zero-filled.
 */
typedef struct geocon_file_hdr GEOCON_FILE_HDR;
struct geocon_file_hdr
{
   int           magic;            /*!< Magic number                         */
   int           version;          /*!< Header version                       */
   int           hdrlen;           /*!< Header length                        */
   int           reserved;         /*!< Reserved - should be 0               */

   char          info  [GEOCON_HDR_INFO_LEN];  /*!< File description         */
   char          source[GEOCON_HDR_INFO_LEN];  /*!< Source of this data      */
   char          date  [GEOCON_HDR_DATE_LEN];  /*!< "YYYY-MM-DD[ HH:MM:SS]"  */

   int           lat_dir;          /*!< Direction of lat values (S-N or N-S) */
   int           lon_dir;          /*!< Direction of lon values (E-W or W-E) */

   int           nrows;            /*!< Number of rows in data               */
   int           ncols;            /*!< Number of cols in data               */

   double        lat_south;        /*!< South latitude  ( -90 to  +90) degs  */
   double        lat_north;        /*!< North latitude  ( -90 to  +90) degs  */

   double        lon_west;         /*!< West  longitude (-180 to +180) degs  */
   double        lon_east;         /*!< East  longitude (-180 to +180) degs  */

   double        lat_delta;        /*!< Latitude  increment in degrees       */
   double        lon_delta;        /*!< Longitude increment in degrees       */

   double        horz_scale;       /*!< Horizontal units per degree          */
   double        vert_scale;       /*!< Vertical   units per meter           */

   char          from_gcs[GEOCON_HDR_NAME_LEN]; /*!< From geogcs name        */
   char          from_vcs[GEOCON_HDR_NAME_LEN]; /*!< From vertcs name        */
   double        from_semi_major;  /*!< From ellipsoid semi-major axis       */
   double        from_flattening;  /*!< From ellipsoid flattening            */

   char          to_gcs  [GEOCON_HDR_NAME_LEN]; /*!< To   geogcs name        */
   char          to_vcs  [GEOCON_HDR_NAME_LEN]; /*!< To   vertcs name        */
   double        to_semi_major;    /*!< To   ellipsoid semi-major axis       */
   double        to_flattening;    /*!< To   ellipsoid flattening            */
};

#define GEOCON_FILE_HDR_LEN  sizeof(GEOCON_FILE_HDR)

/*---------------------------------------------------------------------------*/
/**
 * GEOCON point
 *
 * This is the format of point data in a GEOCON file.
 */
typedef struct geocon_point GEOCON_POINT;
struct geocon_point
{
   float         lat_value;          /*!< Latitude  shift or error value     */
   float         lon_value;          /*!< Longitude shift or error value     */
   float         hgt_value;          /*!< Height    shift or error value     */
};

/*---------------------------------------------------------------------------*/
/**
 * GEOCON internal header
 */
typedef struct geocon_hdr GEOCON_HDR;
struct geocon_hdr
{
   GEOCON_FILE_HDR  fhdr;          /*!< Cached file header                   */

   char          pathname[GEOCON_MAX_PATH_LEN]; /*!< Cached pathname         */
   int           filetype;         /*!< File type (binary or ascii)          */

   GEOCON_BOOL   flip;             /*!< TRUE to byte-swap data               */
   long          points_start;     /*!< Offset to start of points in file    */
   int           line_count;       /*!< Line count when reading ascii file   */

   /* These values may be different from the file header if
      an extent was specified when loading data.
   */
   int           nrows;            /*!< Number of rows  of data in memory    */
   int           ncols;            /*!< Number of cols  of data in memory    */

   double        lat_min;          /*!< South latitude  of data in memory    */
   double        lat_max;          /*!< North latitude  of data in memory    */
   double        lon_min;          /*!< West  longitude of data in memory    */
   double        lon_max;          /*!< East  longitude of data in memory    */

   /* These values are copied from the file header for convienence.
      However, they may be changed if you want the point data written
      in a different order.
   */
   int           lat_dir;          /*!< Direction of lat values (S-N or N-S) */
   int           lon_dir;          /*!< Direction of lon values (E-W or W-E) */

   /* These values are copied from the file header for convienence. */
   double        lat_delta;        /*!< Latitude  increment in degrees       */
   double        lon_delta;        /*!< Longitude increment in degrees       */

   double        horz_scale;       /*!< Horizontal units per degree          */
   double        vert_scale;       /*!< Vertical   units per meter           */

   /* Values for "phantom cells" around our grid. */
   double        lat_min_ghost;    /*!< South latitude  of minus 1 cell      */
   double        lat_max_ghost;    /*!< North latitude  of plus  1 cell      */
   double        lon_min_ghost;    /*!< West  longitude of minus 1 cell      */
   double        lon_max_ghost;    /*!< East  longitude of plus  1 cell      */

   /* This will be null if data is in memory. */
   FILE *        fp;               /*!< file stream                          */

   /* This should be used if mutex control is needed
      for multi-threaded access to the file when
      transforming points and reading data on-the-fly.
      This mutex does not need to be recursive.
   */
   void *        mutex;            /*!< MUTEX for reading                    */

   /* If reading data on the fly, this is null.
      This array is always stored with points going from SW to NE.
   */
   GEOCON_POINT *points;           /*!< Array of (nrows x ncols) points      */
};

/*---------------------------------------------------------------------------*/
/**
 * Extent struct
 *
 * <p>This struct defines the lower-left and the upper-right
 * corners of an extent to use to cut down the area defined by
 * a grid file.
 *
 * <p>Since shifts are usually very small (on the order of fractions
 * of a second), it doesn't matter which datum the values are on.
 *
 * <p>Note that this struct is used only by this API, and is not part of
 * any GEOCON specification.
 */
typedef struct geocon_extent GEOCON_EXTENT;
struct geocon_extent
{
   /* lower-left  corner */
   double  slat;                 /*!< South latitude  (degrees) */
   double  wlon;                 /*!< West  longitude (degrees) */

   /* upper-right corner */
   double  nlat;                 /*!< North latitude  (degrees) */
   double  elon;                 /*!< East  longitude (degrees) */
};

/*---------------------------------------------------------------------------*/
/* GEOCON error codes                                                        */
/*---------------------------------------------------------------------------*/

#define GEOCON_ERR_OK                  0

#define GEOCON_ERR_NO_MEMORY           1
#define GEOCON_ERR_IOERR               2
#define GEOCON_ERR_NULL_PARAMETER      3

#define GEOCON_ERR_INVALID_EXTENT      4
#define GEOCON_ERR_FILE_NOT_FOUND      5
#define GEOCON_ERR_INVALID_FILE        6
#define GEOCON_ERR_CANNOT_OPEN_FILE    7
#define GEOCON_ERR_UNKNOWN_FILETYPE    8
#define GEOCON_ERR_UNEXPECTED_EOF      9
#define GEOCON_ERR_INVALID_TOKEN_CNT  10

/*---------------------------------------------------------------------------*/
/* GEOCON routines                                                           */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/**
 * Determine whether a filename is for a binary or an ascii file.
 *
 * <p>This is done solely by checking the filename extension.
 * No examination of the file contents (if any) is done.
 *
 * @param pathname The filename to query.
 *
 * @return         One of the following:
 *                 <ul>
 *                   <li>GEOCON_FILE_TYPE_UNK  file type is unknown
 *                   <li>GEOCON_FILE_TYPE_BIN  file type is binary
 *                   <li>GEOCON_FILE_TYPE_ASC  file type is ascii
 *                </ul>
 */
extern int geocon_filetype(
   const char *pathname);

/*---------------------------------------------------------------------------*/
/**
 * Convert a GEOCON error code to a string.
 *
 * Currently, these messages are in English, but this call is
 * designed so a user could implement messages in other languages.
 *
 * @param err_num  The error code to convert.
 * @param msg_buf  Buffer to store error message in.
 *
 * @return         A pointer to an error-message string.
 */
extern const char * geocon_errmsg(
   int  err_num,
   char msg_buf[]);

/*---------------------------------------------------------------------------*/
/**
 * Create an empty GEOCON_HDR object.
 *
 * The header returned will be all zeros except for the first four
 * words, which will be properly set.
 *
 * @return A pointer to an allocated GEOCON_HDR struct or GEOCON_NULL.
 */
extern GEOCON_HDR * geocon_create();

/*---------------------------------------------------------------------------*/
/**
 * Load a GEOCON file into memory.
 *
 * @param pathname   The name of the GEOCON file to load.
 *
 * @param extent     A pointer to an GEOCON_EXTENT struct.
 *                   This pointer may be NULL.
 *                   This is ignored for ascii files.
 *
 * @param load_data  TRUE to read shift data into memory.
 *                   A TRUE value will also result in closing the file
 *                   after reading, since there is no need to keep it open.
 *
 * @param prc        A pointer to a result code.
 *                   This pointer may be NULL.
 *                   <ul>
 *                     <li>If successful,   it will be set to GEOCON_ERR_OK (0).
 *                     <li>If unsuccessful, it will be set to GEOCON_ERR_*.
 *                   </ul>
 *
 * @return           A pointer to a GEOCON object or NULL if unsuccessful.
 */
extern GEOCON_HDR * geocon_load(
   const char    *pathname,
   GEOCON_EXTENT *extent,
   GEOCON_BOOL    load_data,
   int           *prc);

/*---------------------------------------------------------------------------*/
/**
 * Write out a GEOCON object to a file.
 *
 * This call can also be used to write out a binary file for an object
 * that was read from an ascii file, and vice-versa.
 *
 * @param hdr        A pointer to a GEOCON_HDR object.
 *
 * @param pathname   The pathname of the file to write.
 *                   This can name either a binary or an ascii file.
 *
 * @param byte_order Byte order of the output file (GEOCON_ENDIAN_*) if
 *                   binary.  A value of GEOCON_ENDIAN_INP_FILE means to
 *                   write the file using the same byte-order as the
 *                   input file if binary or in native byte-order if the
 *                   input file was an ascii file.
 *                   This parameter is ignored when writing ascii files.
 *
 * @param prc        A pointer to a result code.
 *                   This pointer may be NULL.
 *                   <ul>
 *                     <li>If successful,   it will be set to GEOCON_ERR_OK (0).
 *                     <li>If unsuccessful, it will be set to GEOCON_ERR_*.
 *                   </ul>
 *
 * @return           0 if OK, -1 if error.
 */
extern int geocon_write(
   const GEOCON_HDR *hdr,
   const char       *pathname,
   int               byte_order,
   int              *prc);

/*---------------------------------------------------------------------------*/
/**
 * Delete a GEOCON object
 *
 * This method will also close any open stream (and mutex) in the object.
 *
 * @param hdr        A pointer to a GEOCON object.
 */
extern void geocon_delete(
   GEOCON_HDR *hdr);

/*---------------------------------------------------------------------------*/
/**
 * List the contents of a GEOCON header.
 *
 * This routine provides a terse single-line summary of a file header.
 *
 * @param hdr          A pointer to a GEOCON_HDR object.
 *
 * @param fp           The stream to dump it to, typically stdout.
 *                     If NULL, no dump will be done.
 *
 * @param do_hdr_line  TRUE to output a header line.
 */
extern void geocon_list_hdr(
   const GEOCON_HDR *hdr,
   FILE             *fp,
   GEOCON_BOOL       do_hdr_line);

/*---------------------------------------------------------------------------*/
/**
 * Dump the contents of a GEOCON header.
 *
 * This routine provides a verbose multi-line dump of a file header.
 *
 * @param hdr          A pointer to a GEOCON_HDR object.
 *
 * @param fp           The stream to dump it to, typically stdout.
 *                     If NULL, no dump will be done.
 */
extern void geocon_dump_hdr(
   const GEOCON_HDR *hdr,
   FILE             *fp);

/*---------------------------------------------------------------------------*/
/**
 * Dump the contents the GEOCON data.
 *
 * @param hdr          A pointer to a GEOCON_HDR object.
 *
 * @param fp           The stream to dump it to, typically stdout.
 *                     If NULL, no dump will be done.
 *
 * <p>Note that the data is always dumped with latitudes going south-to-north
 * and longitudes going west-to-east.
 */
extern void geocon_dump_data(
   const GEOCON_HDR *hdr,
   FILE             *fp);

/*---------------------------------------------------------------------------*/
/**
 * Perform a forward transformation on an array of points.
 *
 * @param hdr         A pointer to a GEOCON_HDR object.
 *
 * @param interp      The interpolation method to use:
 *                    <ul>
 *                      <li>GEOCON_INTERP_DEFAULT     (biquadratic)
 *                      <li>GEOCON_INTERP_BILINEAR
 *                      <li>GEOCON_INTERP_BICUBIC
 *                      <li>GEOCON_INTERP_BIQUADRATIC
 *                      <li>GEOCON_INTERP_NATSPLINE
 *                    </ul>
 *
 * @param deg_factor  The conversion factor to convert the given coordinates
 *                    to decimal degrees.
 *                    The value is degrees-per-unit.
 *
 * @param hgt_factor  The conversion factor to convert the given height
 *                    to meters.
 *                    The value is meters-per-unit.
 *
 * @param n           Number of points in the array to be transformed.
 *
 * @param coord       An array of GEOCON_COORD values to be transformed.
 *
 * @param h           An array of heights to transform. This may be NULL.
 *
 * @return            The number of points successfully transformed.
 *
 * <p>Note that the return value is the number of points successfully
 * transformed, and points that can't be transformed (usually because
 * they are outside of the grid) are left unchanged.  However, there
 * is no indication of which points were changed and which were not.
 * If you need that information, then call this routine with one point
 * at a time. The overhead for doing that is minimal.
 */
extern int geocon_forward(
   const GEOCON_HDR *hdr,
   int               interp,
   double            deg_factor,
   double            hgt_factor,
   int               n,
   GEOCON_COORD      coord[],
   double            h[]);

/*---------------------------------------------------------------------------*/
/**
 * Perform an inverse transformation on an array of points.
 *
 * @param hdr         A pointer to a GEOCON_HDR object.
 *
 * @param interp      The interpolation method to use:
 *                    <ul>
 *                      <li>GEOCON_INTERP_DEFAULT     (biquadratic)
 *                      <li>GEOCON_INTERP_BILINEAR
 *                      <li>GEOCON_INTERP_BICUBIC
 *                      <li>GEOCON_INTERP_BIQUADRATIC
 *                      <li>GEOCON_INTERP_NATSPLINE
 *                    </ul>
 *
 * @param deg_factor  The conversion factor to convert the given coordinates
 *                    to decimal degrees.
 *                    The value is degrees-per-unit.
 *
 * @param hgt_factor  The conversion factor to convert the given height
 *                    to meters.
 *                    The value is meters-per-unit.
 *
 * @param n           Number of points in the array to be transformed.
 *
 * @param coord       An array of GEOCON_COORD values to be transformed.
 *
 * @param h           An array of heights to transform. This may be NULL.
 *
 * @return            The number of points successfully transformed.
 *
 * <p>Note that the return value is the number of points successfully
 * transformed, and points that can't be transformed (usually because
 * they are outside of the grid) are left unchanged.  However, there
 * is no indication of which points were changed and which were not.
 * If you need that information, then call this routine with one point
 * at a time. The overhead for doing that is minimal.
 */
extern int geocon_inverse(
   const GEOCON_HDR *hdr,
   int               interp,
   double            deg_factor,
   double            hgt_factor,
   int               n,
   GEOCON_COORD      coord[],
   double            h[]);

/*---------------------------------------------------------------------------*/
/**
 * Perform a forward or inverse transformation on an array of points.
 *
 * @param hdr         A pointer to a GEOCON_HDR object.
 *
 * @param interp      The interpolation method to use:
 *                    <ul>
 *                      <li>GEOCON_INTERP_DEFAULT     (biquadratic)
 *                      <li>GEOCON_INTERP_BILINEAR
 *                      <li>GEOCON_INTERP_BICUBIC
 *                      <li>GEOCON_INTERP_BIQUADRATIC
 *                      <li>GEOCON_INTERP_NATSPLINE
 *                    </ul>
 *
 * @param deg_factor  The conversion factor to convert the given coordinates
 *                    to decimal degrees.
 *                    The value is degrees-per-unit.
 *
 * @param hgt_factor  The conversion factor to convert the given height
 *                    to meters.
 *                    The value is meters-per-unit.
 *
 * @param n           Number of points in the array to be transformed.
 *
 * @param coord       An array of GEOCON_COORD values to be transformed.
 *
 * @param h           An array of heights to transform. This may be NULL.
 *
 * @param direction   The direction of the transformation
 *                    (GEOCON_CVT_FORWARD or GEOCON_CVT_INVERSE).
 *
 * @return            The number of points successfully transformed.
 *
 * <p>Note that the return value is the number of points successfully
 * transformed, and points that can't be transformed (usually because
 * they are outside of the grid) are left unchanged.  However, there
 * is no indication of which points were changed and which were not.
 * If you need that information, then call this routine with one point
 * at a time. The overhead for doing that is minimal.
 */
extern int geocon_transform(
   const GEOCON_HDR *hdr,
   int               interp,
   double            deg_factor,
   double            hgt_factor,
   int               n,
   GEOCON_COORD      coord[],
   double            h[],
   int               direction);

/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* LIBGEOCON_H_INCLUDED */
