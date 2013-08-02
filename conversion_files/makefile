# ------------------------------------------------------------------------- #
# Copyright 2013 Esri                                                       #
#                                                                           #
# Licensed under the Apache License, Version 2.0 (the "License");           #
# you may not use this file except in compliance with the License.          #
# You may obtain a copy of the License at                                   #
#                                                                           #
#     http://www.apache.org/licenses/LICENSE-2.0                            #
#                                                                           #
# Unless required by applicable law or agreed to in writing, software       #
# distributed under the License is distributed on an "AS IS" BASIS,         #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  #
# See the License for the specific language governing permissions and       #
# limitations under the License.                                            #
# ------------------------------------------------------------------------- #

# ------------------------------------------------------------------------- #
# makefile for the geocon library conversion files                          #
#                                                                           #
# The only purpose of this makefile is to produce a source list.            #
# ------------------------------------------------------------------------- #

# ------------------------------------------------------------------------
# Setup our make environment
#
TOP_DIR := ..
include $(TOP_DIR)/build/make_includes/make.i

# ------------------------------------------------------------------------
# source files
#
OTHER := \
  gc_nad83_2007_2011_alaska_errors.txt \
  gc_nad83_2007_2011_alaska_shifts.txt \
  gc_nad83_2007_2011_conus_errors.txt  \
  gc_nad83_2007_2011_conus_shifts.txt  \
  gc_nad83_2007_2011_prvi_errors.txt   \
  gc_nad83_2007_2011_prvi_shifts.txt   \
  gc_nad83_harn_2007_alaska_errors.txt \
  gc_nad83_harn_2007_alaska_shifts.txt \
  gc_nad83_harn_2007_conus_errors.txt  \
  gc_nad83_harn_2007_conus_shifts.txt  \
  gc_nad83_harn_2007_prvi_errors.txt   \
  gc_nad83_harn_2007_prvi_shifts.txt   \
  $(NULL)

DIRSRC  := $(OTHER)
ALLSRC  := makefile $(DIRSRC)

# ------------------------------------------------------------------------
# build targets
#
all     :
compile :
link    :

# ------------------------------------------------------------------------
# list all files
#
srclist :
	@ for i in $(ALLSRC); \
	  do echo conversion_files/$$i; \
	  done

# ------------------------------------------------------------------------
# clean targets
#
clean :
