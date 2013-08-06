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
# makefile for the libgeocon library                                        #
# ------------------------------------------------------------------------- #

# ------------------------------------------------------------------------
# Setup our make environment
#
TOP_DIR := ..
include $(TOP_DIR)/build/make_includes/make.i

# ------------------------------------------------------------------------
# program targets
#
TGTBASE  := geocon
LIBNAME  := lib$(TGTBASE)$(STATIC_LIB_SUFFIX)
DLLNAME  := lib$(TGTBASE)
TGT_LIB  := $(LIB_DIR)/$(LIBNAME).$(LIB_EXT)
TGT_DLL  := $(DLL_DIR)/$(DLLNAME).$(DLL_EXT)

TARGETS = \
  $(TGT_LIB) \
  $(TGT_DLL) \
  $(NULL)

# ------------------------------------------------------------------------
# program options
#
CINCS    := -I../include
CFLAGS   := $(COPTS)  $(OPTS) $(CDEFS) $(CINCS)
LDFLAGS  := $(LDOPTS) $(OPTS) $(DLLDEFS)
RCFLAGS  := $(RCDEFS) $(CINCS)

DEF_OPTS := -def:libgeocon.def

# ------------------------------------------------------------------------
# object files
#
C_OBJS   := \
  libgeocon.$(OBJ_EXT) \
  $(NULL)

LIB_OBJS := $(C_OBJS)

LIB_OBJS := $(addprefix $(INT_DIR)/, $(LIB_OBJS))
DLL_OBJS := $(LIB_OBJS)

ifeq ($(OS_TYPE), windows)
  DLL_OBJS += $(addprefix $(INT_DIR)/, libgeocon.res)
endif

ALL_OBJS := $(DLL_OBJS)

# ------------------------------------------------------------------------
# source files
#
C_SRC := $(C_OBJS:.$(OBJ_EXT)=.c)

HDRS  := \
  $(NULL)

OTHER := \
  libgeocon.def \
  libgeocon.i   \
  libgeocon.rc  \
  $(NULL)

CHSRC   := $(C_SRC) $(HDRS)
DIRSRC  := $(CHSRC) $(OTHER)
ALLSRC  := makefile $(DIRSRC)

# ------------------------------------------------------------------------
# build targets
#
all : compile link

compile : $(ALL_OBJS)

link : $(TARGETS)

# ------------------------------------------------------------------------
# Link targets
#
$(TGT_LIB) : $(LIB_OBJS)
	@ $(MK_LIB)

$(TGT_DLL) : $(DLL_OBJS)
	@ $(MK_SHRPP)

# ------------------------------------------------------------------------
# dependencies
#
$(ALL_OBJS) : $(HDRS) ../include/libgeocon.h

# ------------------------------------------------------------------------
# list all files
#
srclist :
	@ for i in $(ALLSRC); \
	  do echo src/$$i; \
	  done

# ------------------------------------------------------------------------
# clean targets
#
clean : notarget
	-$(RD) $(INT_DIR)

notarget :
	-$(RM) $(TARGETS)
