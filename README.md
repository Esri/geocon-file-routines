## geocon-file-routines

### Introduction

The National Geodetic Survey (NGS) has released the GEOCON set of
tools for performing three-dimensional coordinate transformations. These
tools include two sets of binary grid files: one set for transforming
points from NAD83-HARN to NAD83-NSRS2007, and the other set for
transforming points from NAD83-NSRS2007 to NAD83-2011.

Each of these file sets consist of nine files, three files each for the
Alaska, CONUS, and Puerto Rico/Virgin Islands (PR/VI) regions. The three
files contain corresponding latitude, longitude, and height shifts.

Note that there are no NAD83-NSRS2007 or NAD83-2011 coordinate systems
defined for Hawaii, only NAD83-HARN coordinate systems, thus there are
no GEOCON files for that area.

The NGS tools also contain error files, documentation files, and programs
for transforming points.

For further information about the NGS GEOCON tools, go to
[here](http://beta.ngs.noaa.gov/GEOCON/).

The purposes of this package is to provide the following:

   1. Provide new GEOCON files that combine the single-grid latitude,
      longitude, and height files into a single multi-grid file for each
      transformation/region. These files may be ascii or binary files.

   2. Provide a C API for reading and writing GEOCON files, and using
      a GEOCON file to transform points.

   3. Provide sample programs that use this API:

      <pre>
         geocon_file  to read/write/dump GEOCON files
         geocon_cmb   to combine corresponding single-grid GEOCON files
                      into a multi-grid file
         geocon_cvt   to use a specified GEOCON file to convert points
      </pre>

### Issues with the existing GEOCON implementation

When evaluating the existing GEOCON package from a GIS persepective, we
found the following issues:

1.  Unlike the HARN and NADCON transformation files, which are named
    "*.las" and "*.los" for the latitude and longitude shift files and in
    which each file pair has the same basename part, the GEOCON files are
    all named *.b, with different basename parts for each grid.

    Thus, where the NADCON Alaska files are "alaska.las" and "alaska.los",
    the corresponding GEOCON files are "gslaa.b", "gsloa.b", and "gsva.b".

    Having a common basename for all the sub-files makes it easier to use,
    as only one name (part) is required. Better still is to have only a
    single, multi-grid file, since one would never use only one of the
    single-grid files at one time. This is especially important when a
    database needs to specify the filename(s) needed for a given
    transformation.

    Also, the filenames themselves seem to be a throwback to a time when
    filenames were limited to eight characters. Thus, we get the elaborate
    scheme to encode "grid shift latitude alaska 2007-2011 binary" as
    "gslaa11.b" and "grid error height conus harn-2007 binary" as
    "gev.b" (note the missing area and version). It would be better to use
    longer, more descriptive names.  For example:

   <pre>
       gslaa11.b    -> gc_nad83_2007_2011_alaska_shifts.lat
       gev.b        -> gc_nad83_harn_2007_conus_errors.hgt
   </pre>

   Or, even better would be to combine the corresponding latitude,
   longitude, and height files into one multi-grid file.

   For example:

   <pre>
       gslaa11.b
       gsloa11.b
       gsva11.b     -> gc_nad83_2007_2011_alaska_shifts.gcb

       gela.b
       gelo.b
       gev.b        -> gc_nad83_harn_2007_conus_errors.gcb
   </pre>

2.  In a GIS environment, it is easier to specify and use a single file
    for transformations than to load three files. This is especially
    important if you don't want to load the file contents into memory
    but just read point-shift values from the file as needed. In this case,
    it is better to keep one file open than three.

3.  All of these files are written as FORTRAN "unformatted" files, rather
    than as "binary" files. This means each "record" has prefix/suffix
    words describing the size of that record.

    In a C environment, these extra words are not needed, and in a FORTRAN
    environment, these prefix/suffix words would not be needed if the
    files were written as "binary" instead of "unformatted" files, or
    processed as a "direct" access file with a record length of four.

4.  The header portion of the file is insufficient in describing the file
    contents and its applicability.

5.  The provided "geocon" program is not very useful, as it is designed
    to only read input data in a specific format (NGS Blue Book *80* and *86*
    "80-column card format" records), and output values in the same format.
    There should have been at least an option to input and output free-form
    input data.

    The same program is interactive, and thus cannot be used in any kind
    of automated environment.

    The "geocon" program is also written in FORTRAN rather than in C,
    which would lend itself easier to being modified, or imported into
    a GIS environment (which is usually written in C/C++).

    The program is also tied to reading either big- or little-endian
    files depending on the hardware/software platform, rather than being
    able to read either version on any platform. It also hard-codes
    absolute pathnames for the files, and thus separate programs (which
    differ only in the absolute pathnames and some comments) are needed
    for the harn-2007 conversions and the 2007-2011 conversions.

6.  The provided "geocon" programs are also incorrect in calculating an
    inverse value, as it only subtracts the calculated forward-shift once,
    instead of iteratively zeroing in on the correct value. You cannot
    just subtract the forward-shift value from a point to get the
    original point, since that shift value is the value to be used to
    shift the new point forward, and not the value to shift back
    to the old point.

    This can be easily shown by doing a "round trip" (transforming a point
    forwards then transforming it back). You should get back the original
    point.

### Other GIS considerations

A GIS environment is much different than a scientific environment. In a
GIS environment (like in drawing a map), it is common to transform
thousands or even millions of points at a time. In such a situation,
one is not interested whether an individual point successfully transformed
or what the margin of error is. It is either transformed or not
(probably because of being outside the grid area), and the point is just
used as it is.

Thus, the error files are not of any use, and neither are the transform
quality files. In fact, the area covered by the CONUS grid file is
15,022,142 sqkm, whereas the area covered by the CONUS horizontal
transform-quality records is 800 sqkm (32 records * 5km * 5km)
or .005% of the grid area, and the area covered by the CONUS vertical
transform-quality records is 1625 sqkm (65 records * 5km * 5km)
or 0.01% of the grid area.

Knowing all this information may be of vital importance to the providers
of the grid files, but is not needed by the users of the grid files.
Thus, this package only deals with the grid-shift files and does not use
any grid-error files or transform-quality files (although the API can be
used to read and/or write grid-error files).

### A proposed new multi-grid GEOCON file

The filename extension identifies the format of the contents, as follows:

<pre>
   .gcb  "Geocon Combined Binary"   (referred to as a GCB file)
   .gca  "Geocon Combined Ascii"    (referred to as a GCA file)
</pre>

Both a GCB file and a GCA file have the same contents, but the syntax is
different. Note that either format can be used to store either shift or
error values.

#### Binary (GCB) file

The syntax of a binary header is:

<pre>
   Field name       Type      Description
   ---------------  --------  --------------------------------------
   magic            int       File magic number (0x47434f4e) ('GCON')
   version          int       Header version
   hdrlen           int       Header length
   reserved         int       Reserved - should be 0

   info             char[80]  File description
   source           char[80]  Source of this data
   date             char[24]  Date: "YYYY-MM-DD[ HH:MM:SS]"

   lat_dir          int       Dir of lat values (S-N or N-S) (0 or 1)
   lon_dir          int       Dir of lon values (W-E or E-W) (0 or 1)

   nrows            int       Number of rows in data
   ncols            int       Number of cols in data

   lat_south        double    South latitude  ( -90 to  +90) degs
   lat_north        double    North latitude  ( -90 to  +90) degs

   lon_west         double    West  longitude (-180 to +180) degs
   lon_east         double    East  longitude (-180 to +180) degs

   lat_delta        double    Latitude  increment in degrees
   lon_delta        double    Longitude increment in degrees

   horz_scale       double    Horizontal units per degree
   vert_scale       double    Vertical   units per meter

   from_gcs         char[80]  From geographic coordinate system name
   from_vcs         char[80]  From vertical   coordinate system name
   from_semi_major  double    From ellipsoid semi-major axis
   from_flattening  double    From ellipsoid inverse flattening

   to_gcs           char[80]  To   geographic coordinate system name
   to_vcs           char[80]  To   vertical   coordinate system name
   to_semi_major    double    To   ellipsoid semi-major axis
   to_flattening    double    To   ellipsoid inverse flattening
   ---------------            ----------------------
   total size                 632 bytes
</pre>

   All string lengths include the nul-terminator. String fields should be
   zero-padded on the right.

   Note that by checking the magic number, a program can not only verify
   that the file is indeed a GCB file, but also whether byte-swapping
   is needed (the magic number will either be 'GCON' or 'NOCG').

   The data section consists of (nrows * ncols) data points:

<pre>
   Field name       Type      Description
   ---------------  --------  --------------------------------------
   lat_value        float     Latitude  shift-or-error value
   lon_value        float     Longitude shift-or-error value
   hgt-value        float     Height    shift-or-error value
</pre>

#### ASCII (GCA) file

Rules for ascii files are:

   1.  Anything following a # will be considered a comment and will
       be discarded.

   2.  Whitespace is used to delimit tokens in each line, and extraneous
       whitespace outside of quotes is ignored.

   3.  Blank lines are discarded. This is done after removing any comments
       and all leading and trailing whitespace.

   4.  Any string value longer than the field width is silently truncated.
       Maximum field withs are:
          info      79 chars
          source    79 chars
          date      23 chars
          from_gcs  79 chars
          from_vcs  79 chars
          to_gcs    79 chars
          to_vcs    79 chars

   5.  Header line entries have the syntax:
          field-name  field-value

       Point line entries have the syntax:
          latitude-value longitude-value height-value

   6.  All floating point numbers are displayed with a '.' as the
       decimal separator. This is because these files could be used
       anywhere, and so should be locale-neutral.

       Text files will be read in properly even if decimal separators
       other than a '.' are used (and they match the separators for
       the current locale), but they will always be written out using a
       decimal point. This allows ease in creating an ascii file using
       programs that can't output locale-neutral number strings.

   7.  String values should be enclosed in quotes if they contain any
       whitespace.

The syntax of an ASCII header is:

<pre>
   Token            Description
   ---------------  --------------------------------------
   info             "Description"
   source           "Source of the data"
   date             "YYYY-MM-DD"

   lat_dir          "S-N" or "N-S"
   lon_dir          "W-E" or "E-W"

   nrows            number of rows
   ncols            number of columns

   lat_south        South latitude
   lat_north        North latitude

   lon_west         West  longitude
   lon_east         East  longitude

   lat_delta        Latitude  increment
   lon_delta        Longitude increment

   horz_scale       horizontal units-per-degree
   vert_scale       vertical   units-per-meter

   from_gcs         From geographic coordinate system name
   from_vcs         From vertical   coordinate system name
   from_semi_major  From ellipsoid semimajor axis
   from_flattening  From ellipsoid inverse flattening

   to_gcs           To   geographic coordinate system name
   to_vcs           To   vertical   coordinate system name
   to_semi_major    To   ellipsoid semimajor axis
   to_flattening    To   ellipsoid inverse flattening
</pre>

The data section consists of (nrows * ncols) lines:

<pre>
   latitude-shift  longitude-shift  height-shift
</pre>

Example (from the gc_nad83_2007_2011_prvi_shifts.gca file):

<pre>
   info             "GEOCON - PR/VI shifts: NAD83-NSRS2007 to NAD83-2011"
   source           "Combines the NGS files: dslap11.b dslop11.b dsvp11.b"
   date             "2012-08-12"

   lat_dir          S-N
   lon_dir          W-E
   nrows            181
   ncols            361

   lat_south        17
   lat_north        20
   lon_west         -68
   lon_east         -62
   lat_delta        0.016666666666666666
   lon_delta        0.016666666666666666

   horz_scale       360000000
   vert_scale       100

   from_gcs         "NAD_1983_NSRS2007"
   from_vcs         "NAD_1983"
   from_semi_major  6378137
   from_flattening  298.25722210100002

   to_gcs           "NAD_1983_2011"
   to_vcs           "NAD_1983"
   to_semi_major    6378137
   to_flattening    298.25722210100002

   105.0           500.0           1.299999952
   104.999984741   500.000061035   1.299992442
   ...
</pre>

### The "geocon_file" program

The "geocon_file" program provides the ability to manipulate GEOCON files,
providing the ability to copy, dump, list, transform, and convert
the contents of such files.

For example, since it is easier to write an ascii file rather than
a binary file, a person could easily create an ascii file containing
the shifts he wanted, then use this program to transform it to a binary
format for distribution and consumption.

The usage (from executing "geocon_file -help") is:

<pre>
   Usage: geocon_file [options] file ...
   Options:
     -?, -help  Display help

     -l         List header info
     -h         Dump header info
     -d         Dump shift data

     -B         Write    big-endian binary file
     -L         Write little-endian binary file
     -N         Write native-endian binary file
                  (default is same as input file)

     -o file    Specify output file
     -e slat wlon nlat elon   Specify extent
</pre>

Note that some GEOCON files are quite large. Tha Alaska files are 84MB, and
the CONUS files are 63MB. Reading them into memory can take up a lot of
RAM, which may be limited. Thus, this program also includes the option of
creating a new, smaller file that is cut down by a specified extent.
This may be of significant use for inclusion in mobile environments
where available memory may be at a premium.

This program can be used to create a binary file from an ascii file
(GCA -> GCB), to create an ascii file from a binary file (GCB -> GCA),
or to create an opposite-endian binary file (GCB -> GCB).

If the program is used to copy one file to another, then only one input
file can be named on the command line. If it is used to list, dump,
or validate files, then multiple files can be specified.

### The "geocon_cmb" program

The "geocon_cmb" program will combine three single-grid GEOCON files into
a multi-grid GEOCON file.

The usage (from executing "geocon_cmb -help") is:

<pre>
   Usage: geocon_cmb [options] file ...
   Options:
     -?, -help  Display help

     -B         Write    big-endian binary file
     -L         Write little-endian binary file
     -N         Write native-endian binary file
                  (default is same as input file)
</pre>

Each file specified on the command line is a conversion file containing
the information needed to convert the three single-grid files into a
multi-grid file.

The format of this file is:

<pre>
      Information                            (max of 79 characters)
      Source                                 (max of 79 characters)
      Date                                   (YYYY-MM-DD)
      From GCS name                          (max of 79 characters)
      To   GCS name                          (max of 79 characters)
      Output path of created   asc/bin file  (*.gcb or *.gca)
      Input  path of latitude  binary  file  (dsla*.b or dela*.b)
      Input  path of longitude binary  file  (dslo*.b or delo*.b)
      Input  path of height    binary  file  (dsv*.b  or dev*.b)
</pre>

Example (from the gc_nad83_2007_2011_prvi_shifts.txt):

<pre>
      GEOCON - PR/VI shifts: NAD83-NSRS2007 to NAD83-2011
      Combines the NGS files: dslap11.b dslop11.b dsvp11.b
      2012-08-12
      NAD_1983_NSRS2007
      NAD_1983_2011
      shift_files_bin/gc_nad83_2007_2011_prvi_shifts.gcb
      grid11_pc/dslap11.b
      grid11_pc/dslop11.b
      grid11_pc/dsvp11.b
</pre>

Note that the directory names for the source and target files may
need to be changed.

### The "geocon_cvt" program

The "geocon_cvt" program is a sample program illustrating how to use
GEOCON files and the geocon transform routines to do either a forward
or an inverse transformation.

The usage (from executing "geocon_cvt -help") is:

<pre>
   Usage: geocon_cvt [options] filename [lat lon hgt] ...
   Options:
     -?, -help  Display help
     -r         Reversed data: (lon lat hgt) instead of (lat lon hgt)
     -k         Read and write *80*/*86* records
     -d         Read shift data on the fly (no load of data)
     -f         Forward transformation           (default)
     -i         Inverse transformation
     -R         Do round trip

     -L         Use bilinear       interpolation
     -C         Use bicubic        interpolation
     -N         Use natural spline interpolation
     -Q         Use biquadratic    interpolation (default)
     -A         Use all interpolation methods

     -c value   Conversion: degrees-per-unit     (default is 1)
     -h value   Conversion: meters-per-unit      (default is 1)
     -s string  Use string as output separator   (default is " ")
     -p file    Read points from file            (default is "-" or stdin)
     -e slat wlon nlat elon   Specify an extent
     -E slat wlon nlat elon   Specify an extent for data dump only

   If no coordinate triples are specified on the command line,
   then they are read one per line from the specified data file.
</pre>

If an extent is specified, only that part of the file will be loaded
into memory. This option is ignored if the read-shift-data-on-the-fly
(-d) option is also specified.

The input coordinates are free-form numbers separated by whitespace. Also,
if the decimal point character is not a comma then any commas in the
input is converted to whitespace.

This program can use either a GCB or a GCA file, so it can be useful
in testing an ascii file prior to converting it to a binary file
for distribution. (But remember that ascii files are slow to load.)

Note that the "-k" (Read and write *80*/*86* records) option tells the
program to process input and output the same as the FORTRAN program does
(but without using the ancillary files).

Examples of use:

<pre>
  geocon_cvt -f gc_nad83_2007_2011_conus_shifts.gcb 40.201 -110.64 6.78
  >> 40.20100011852538 -110.6399998024828 6.766488393273363

  geocon_cvt -i gc_nad83_2007_2011_conus_shifts.gcb 40.201 -110.64 6.78
  >> 40.20099988147462 -110.6400001975173 6.793511602169226

  geocon_cvt -f gc_nad83_2007_2011_conus_shifts.gcb 40.201 -110.64 6.78 |
  geocon_cvt -i gc_nad83_2007_2011_conus_shifts.gcb
  >> 40.201 -110.64 6.779999999999999

  geocon_cvt -A -R gc_nad83_2007_2011_conus_shifts.gcb 40.201 -110.64 6.78
  >> bilinear    : --> 40.20100011852753 -110.6399998024765 6.76648888188839
  >> bilinear    : <-- 40.201 -110.64 6.78
  >>
  >> bicubic     : --> 40.20100011852494 -110.6399998024849 6.766488146399679
  >> bicubic     : <-- 40.201 -110.64 6.78
  >>
  >> natspline   : --> 40.2010001185394  -110.6399998025955 6.766469999644403
  >> natspline   : <-- 40.201 -110.64 6.78
  >>
  >> biquadratic : --> 40.20100011852538 -110.6399998024828 6.766488393273363
  >> biquadratic : <-- 40.201 -110.64 6.78
</pre>

### The geocon API

The following calls are in the API:

<pre>
   geocon_filetype()    Determine the filetype of a GEOCON file (bin or asc)
   geocon_errmsg()      Convert an error code to a string

   geocon_create()      Create an empty GEOCON_HDR object
   geocon_load()        Load   a GEOCON file into a GEOCON_HDR object
   geocon_write()       Write  a GEOCON file from a GEOCON_HDR object
   geocon_delete()      Delete a GEOCON_HDR object

   geocon_list_hdr()    List the contents   of a GEOCON_HDR
   geocon_dump_hdr()    Dump the contents   of a GEOCON_HDR
   geocon_dump_data()   Dump the shift data in a GEOCON_HDR

   geocon_forward()     Do a  forward transformation on an array of points
   geocon_inverse()     Do an inverse transformation on an array of points
   geocon_transform()   Do a  fwd/inv transformation on an array of points
</pre>

This library is documented in detail [here](
https://raw.github.com/Esri/geocon-file-routines/master/doc/html/index.html).

### Resources

* [NGS GEOCON site](http://beta.ngs.noaa.gov/GEOCON/)

### Issues

Find a bug or want to request a new feature?  Please let us know by submitting
an issue.

### Contributing

Anyone and everyone is welcome to contribute.

### Licensing

Copyright 2010-2013 Esri

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

A copy of the license is available in the repository's [license.txt](
https://raw.github.com/Esri/geocon-file-routines/master/license.txt) file.

[](Esri Tags: GEOCON transformation NAD83)
[](Esri Language: C)
