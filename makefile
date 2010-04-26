#
# Make file for compiling host app
#
# (C) Copyright 2008-2010
# Texas Instruments, <www.ti.com>
# Nishanth Menon <nm@ti.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation version 2.
#
# This program is distributed .as is. WITHOUT ANY WARRANTY of any kind,
# whether express or implied; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA

HOSTOS := $(shell uname -s | tr '[:upper:]' '[:lower:]' | \
	    sed -e "s/\(cygwin\).*/cygwin/")
srctree := $(if $(SRC),$(SRC),$(CURDIR))

ifeq ($(HOSTOS),windowsnt)
WINDOWS=1
endif

ifeq ($(HOSTOS),windows32)
WINDOWS=1
endif

# Windows Specific handling
ifdef WINDOWS
EXE_PREFIX=.exe
#Installation System specific
ifndef COMPILER_PREFIX
COMPILER_PREFIX=C:\\MinGW\\bin\\
endif
ifndef APP_PREFIX
APP_PREFIX=C:\\GnuWin32\\bin\\
endif
endif #windows

# Selective Library
ifdef WINDOWS
LIB_FILES=lib/serial_win32.c lib/file_win32.c
else
LIB_FILES=lib/serial_posix.c lib/file_posix.c
endif
LIB_FILES+=lib/f_status.c lib/lcfg/lcfg_static.c

#App source code
PSERIAL_FILES=src/pserial.c
KERMIT_FILES=src/ukermit.c
UCMD_FILES=src/ucmd.c
PUSB_FILES=src/pusb.c
GPSIGN_FILES=src/gpsign.c
TAGGER_FILES=src/tagger.c

# Add all SOC/platform specific sty files here
STY_FILES=src/asm/sty-omap3.S

DOCS=docs/html docs/latex

# Final output files
PSERIAL_EXE=pserial$(EXE_PREFIX)
KERMIT_EXE=ukermit$(EXE_PREFIX)
UCMD_EXE=ucmd$(EXE_PREFIX)
PUSB_EXE=pusb$(EXE_PREFIX)
GPSIGN_EXE=gpsign$(EXE_PREFIX)
TAGGER_EXE=tagger$(EXE_PREFIX)

# Object Files
PSERIAL_OBJ=$(PSERIAL_FILES:.c=.o)
KERMIT_OBJ=$(KERMIT_FILES:.c=.o)
UCMD_OBJ=$(UCMD_FILES:.c=.o)
PUSB_OBJ=$(PUSB_FILES:.c=.o)
GPSIGN_OBJ=$(GPSIGN_FILES:.c=.o)
TAGGER_OBJ=$(TAGGER_FILES:.c=.o)

LIB_OBJ=$(LIB_FILES:.c=.o)
STY_OBJS=$(STY_FILES:.S=.ao)
STY_BINS=$(STY_FILES:.S=.bin)

CLEANUPFILES=$(PSERIAL_OBJ) $(LIB_OBJ) $(PSERIAL_EXE) $(GWART_OBJ)\
			 $(KERMIT_EXE) $(KERMIT_OBJ) $(UCMD_OBJ) $(UCMD_EXE)\
			 $(PUSB_EXE) $(PUSB_OBJ) $(GPSIGN_EXE) $(GPSIGN_OBJ)\
			 $(TAGGER_EXE) $(TAGGER_OBJ) $(STY_OBJS)

CC=$(COMPILER_PREFIX)gcc
LD=$(COMPILER_PREFIX)gcc
RM=$(APP_PREFIX)rm$(EXE_PREFIX)
ECHO=$(APP_PREFIX)echo$(EXE_PREFIX)
DOXYGEN=$(APP_PREFIX)doxygen$(EXE_PREFIX)

CFLAGS=-Wall -O3 -Iinclude -Ilib/lcfg
LDFLAGS=

CFLAGS+=-fdata-sections -ffunction-sections
LDFLAGS+=--gc-sections --print-gc-sections --stdlib
LDFLAGS_USB=-lusb

CROSS_CC ?=$(CROSS_COMPILE)gcc
CROSS_OBJDUMP ?=$(CROSS_COMPILE)objdump
CROSS_OBJCOPY ?=$(CROSS_COMPILE)objcopy

ifdef DISABLE_COLOR
CFLAGS+=-DDISABLE_COLOR
endif

ifdef V
    VERBOSE = $(V)
endif
ifndef VERBOSE
  VERBOSE = 0
endif

%.ao: %.S
	@$(ECHO) "Cross Compiling: " $<
	$(if $(VERBOSE:1=),@)$(CROSS_CC) $(CFLAGS) -o $@ -c $<

%.bin: %.ao %.S
	@$(ECHO) "Generating Crossbin: " $<
	$(if $(VERBOSE:1=),@)$(CROSS_OBJCOPY) -O binary $< $@

%.o: %.c
	@$(ECHO) "Compiling: " $<
	$(if $(VERBOSE:1=),@)$(CC) $(CFLAGS) -o $@ -c $<

.PHONY : all

all: $(PSERIAL_EXE) $(KERMIT_EXE) $(UCMD_EXE) $(GPSIGN_EXE) $(TAGGER_EXE)

usb: $(PUSB_EXE)

sty: $(STY_BINS)
	@$(ECHO) "Completed all sty files"

$(PSERIAL_EXE): $(PSERIAL_OBJ) $(LIB_OBJ) makefile
	@$(ECHO) "Generating:  $@"
	$(if $(VERBOSE:1=),@)$(LD) $(PSERIAL_OBJ) $(LIB_OBJ) $(LDFLAGS) -o $@
	@$(ECHO)

$(KERMIT_EXE): $(KERMIT_OBJ) $(LIB_OBJ) makefile
	@$(ECHO) "Generating:  $@"
	$(if $(VERBOSE:1=),@)$(LD) $(KERMIT_OBJ) $(LIB_OBJ) $(LDFLAGS) -o $@
	@$(ECHO)

$(UCMD_EXE): $(UCMD_OBJ) $(LIB_OBJ) makefile
	@$(ECHO) "Generating:  $@"
	$(if $(VERBOSE:1=),@)$(LD) $(UCMD_OBJ) $(LIB_OBJ) $(LDFLAGS) -o $@
	@$(ECHO)

$(GPSIGN_EXE): $(GPSIGN_OBJ) $(LIB_OBJ) makefile
	@$(ECHO) "Generating:  $@"
	$(if $(VERBOSE:1=),@)$(LD) $(GPSIGN_OBJ) $(LIB_OBJ) $(LDFLAGS) -o $@
	@$(ECHO)

$(TAGGER_EXE): $(TAGGER_OBJ) $(LIB_OBJ) makefile
	@$(ECHO) "Generating:  $@"
	$(if $(VERBOSE:1=),@)$(LD) $(TAGGER_OBJ) $(LIB_OBJ) $(LDFLAGS) -o $@
	@$(ECHO)

$(PUSB_EXE): $(PUSB_OBJ) $(LIB_OBJ) makefile
	@$(ECHO) "Generating:  $@"
	$(if $(VERBOSE:1=),@)$(LD) $(PUSB_OBJ) $(LIB_OBJ) $(LDFLAGS) $(LDFLAGS_USB) -o $@
	@$(ECHO)

.PHONY : docs
docs:
	@$(ECHO) "Generating Documentation:"
	$(if $(VERBOSE:1=),@)-$(DOXYGEN) docs/doxyfile
	$(if $(VERBOSE:1=),@)-$(MAKE) -C docs/latex
.PHONY : clean
clean:
	@$(ECHO) "Cleaning:"
	$(if $(VERBOSE:1=),@)-$(RM) -$(if $(VERBOSE:1=),,v)f $(CLEANUPFILES) core

.PHONY : distclean
distclean:
	@$(ECHO) "Dist cleaning:"
	$(if $(VERBOSE:1=),@)-$(RM) -$(if $(VERBOSE:1=),,v)f $(CLEANUPFILES)
	$(if $(VERBOSE:1=),@)-$(RM) -r$(if $(VERBOSE:1=),,v)f $(DOCS)
ifndef WINDOWS
	$(if $(VERBOSE:1=),@)find $(srctree) $(RCS_FIND_IGNORE) \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
		-o -name '.*.rej' -o -size 0 -o -name "*.ao"\
		-o -name '*%' -o -name '.*.cmd' -o -name 'core' \) \
		-type f -print | xargs rm -$(if $(VERBOSE:1=),,v)f
endif

