##===- Makefile --------------------------------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##

# If GONG_LEVEL is not set, then we are the top-level Makefile. Otherwise, we
# are being included from a subdirectory makefile.

ifndef GONG_LEVEL

IS_TOP_LEVEL := 1
GONG_LEVEL := .
DIRS := include lib tools

PARALLEL_DIRS :=

endif

###
# Common Makefile code, shared by all Gong Makefiles.

# Set LLVM source root level.
LEVEL := $(GONG_LEVEL)/../..

# Include LLVM common makefile.
include $(LEVEL)/Makefile.common

# Set common Gong build flags.
CPP.Flags += -I$(PROJ_SRC_DIR)/$(GONG_LEVEL)/include -I$(PROJ_OBJ_DIR)/$(GONG_LEVEL)/include

###
# Gong Top Level specific stuff.

ifeq ($(IS_TOP_LEVEL),1)

ifneq ($(PROJ_SRC_ROOT),$(PROJ_OBJ_ROOT))
$(RecursiveTargets)::
	$(Verb) for dir in test unittests; do \
	  if [ -f $(PROJ_SRC_DIR)/$${dir}/Makefile ] && [ ! -f $${dir}/Makefile ]; then \
	    $(MKDIR) $${dir}; \
	    $(CP) $(PROJ_SRC_DIR)/$${dir}/Makefile $${dir}/Makefile; \
	  fi \
	done
endif

test::
	@ $(MAKE) -C test

clean::
	@ $(MAKE) -C test clean

.PHONY: test clean

endif
