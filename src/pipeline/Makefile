# This file is part of the Astrometry.net suite.
# Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.
#
# The Astrometry.net suite is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, version 2.
#
# The Astrometry.net suite is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Astrometry.net suite ; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

ifndef MAKEFILE_INCLUDED
MAKEFILE_INCLUDED := 1
#export

# no default rules
.SUFFIXES :=

# don't delete "intermediate" files.
.SECONDARY:

TYCHO2_DIR := tycho2/
                    # (note, trailing '/' required!)
TYCHO2_FILES := catalog.dat suppl_1.dat
TYCHO2 := $(addprefix $(TYCHO2_DIR),$(TYCHO2_FILES))
TYCHO2_URL := http://www.astro.ku.dk/~cf/CD/data/
TYCHO2_PRESENT := $(TYCHO2_DIR)tycho2-present

USNOB_DIR := usnob/
                  # (note, trailing '/' required!)
USNOB_PRESENT := $(USNOB_DIR)usnob-present

TYCHO2_FITS_DIR := tycho2-fits/
                              # (note, trailing '/' required!)
TYCHO2_FITS_PRESENT := $(TYCHO2_FITS_DIR)tycho2-fits-present

USNOB_FITS_DIR := usnob-fits/
                            # (note, trailing '/' required!)
USNOB_FITS_TEMPLATE := "$(USNOB_FITS_DIR)usnob10_hp%03i.fits"
USNOB_FITS_NSIDE := 9
USNOB_FITS_PRESENT := $(USNOB_FITS_DIR)usnob-fits-present

AN_CAT_DIR    := an/
               # (note, a trailing '/' is required!)
AN_TEMPLATE := $(AN_CAT_DIR)an_hp%03i.fits
AN_NSIDE  := 9
AN_CATALOG_PRESENT := $(AN_CAT_DIR)an-catalog-present

NUMS2 := 00 01 02 03 04 05 06 07 08 09 10 11

# name of this index.
INAME         := sdss

IDFILES       := no

TEMP_DIR   := 

TIMESTAMP_DIR := logs/
LOG_DIR       := logs/
LOG_BASE      := $(LOG_DIR)log
export LOG_BASE

CUT_DIR       := cut/
                    # (note, a trailing '/' is required!)
CUT_BASE      := $(CUT_DIR)an-$(INAME)-
CUT_TEMP      := $(TEMP_DIR)an-$(INAME)-
CUT_PREFIXES  := $(addprefix $(CUT_BASE),$(NUMS2))
CUT_OBJS      := $(addsuffix .objs.fits,$(CUT_PREFIXES))
CUT_IDS       := $(addsuffix .id.fits,$(CUT_PREFIXES))
CUT_SKDTS     := $(addsuffix .skdt.fits,$(CUT_PREFIXES))
CUT_FILES     := $(CUT_OBJS) $(CUT_SKDTS)
ifeq ($(IDFILES), yes)
CUT_FILES     += $(CUT_IDS)
endif
CUT_AN_NSIDE  := 1000
CUT_AN_SWEEPS := 12
CUT_AN_DEDUP  := 8
CUT_AN_CUT    := -R
CUT_NLEAF     := 15

INDEX_ID   := 1
INDEX_DIR  := $(INAME)-$(INDEX_ID)/
CATALOG    := $(CUT_BASE)
INDEX      := $(INDEX_DIR)$(INAME)-$(INDEX_ID)-
PHASE1      := $(INDEX_DIR)phase1-
PHASE2      := $(INDEX_DIR)phase2-
TEMP1      := $(TEMP_DIR)tmp.phase1-
TEMP2      := $(TEMP_DIR)tmp.phase2-
TEMPINDEX  := $(TEMP_DIR)tmp.$(INAME)-$(INDEX_ID)-

SDSS_FIELDS := sdss-fields/
SDSS_HOMER  := sdss-fields-homer/

# Total number of fields in sdss-fields/sdssfield01-hp02.xy:
TOTAL_FIELDS := 2618

WGET            := wget --continue
MKDIR           := mkdir -p
TOUCH           := touch
MV              := mv
CP				:= cp
SED             := sed
GREP            := grep
WC              := wc
BC				:= bc -l

AN_DIR          := ../quads/
AN_RUN_DIR      := ./

USNOBTOFITS     := $(AN_RUN_DIR)an-run $(AN_DIR)usnobtofits
TYCHO2TOFITS    := $(AN_RUN_DIR)an-run $(AN_DIR)tycho2tofits
HPOWNED         := $(AN_DIR)hpowned
BUILD_AN        := $(AN_RUN_DIR)an-run $(AN_DIR)build-an-catalog
CUT_AN          := $(AN_RUN_DIR)an-run $(AN_DIR)cut-an
STARTREE        := $(AN_RUN_DIR)an-run $(AN_DIR)startree
UNPERMUTE_QUADS := $(AN_RUN_DIR)an-run $(AN_DIR)unpermute-quads
UNPERMUTE_STARS := $(AN_RUN_DIR)an-run $(AN_DIR)unpermute-stars
CODETREE        := $(AN_RUN_DIR)an-run $(AN_DIR)codetree
HPQUADS         := $(AN_RUN_DIR)an-run $(AN_DIR)hpquads

HOMER           := $(AN_RUN_DIR)an-run $(AN_DIR)homer
BLIND           := $(AN_DIR)blind
PRINTSOLVED     := $(AN_DIR)printsolved

QUADIDX			:= $(AN_RUN_DIR)an-run $(AN_DIR)quadidx
QUADLOCATIONS 	:= $(AN_RUN_DIR)an-run $(AN_DIR)quadlocations 
QUADSPERSTAR 	:= $(AN_RUN_DIR)an-run $(AN_DIR)quadsperstar 
CODEPROJ        := $(AN_RUN_DIR)an-run $(AN_DIR)codeprojections
EPSTOPNG		:= $(AN_DIR)epstopng
MATLAB			:= matlab
PNMTOPNG		:= pnmtopng
PNGTOPNM		:= pngtopnm

% s=4;
% Nside = sqrt((4*pi) / ((s/60*pi/180).^2) / 12)
TEMP_NOQUADS := $(TEMP_DIR)tmp.noquads.rd.fits
NOQUADS      := $(INDEX_DIR)noquads

STARTREE_OPTIONS := -R $(CUT_NLEAF) -b -s -S
CODETREE_OPTS := -R 15 -b -s -S
#HPQUADS_OPTS  := -l 4 -u 5 -n 700 -i $(INDEX_ID) -c -x 6 -y 6 -r 4
#HPQUADS_OPTS  := -l 4 -u 5 -n 1000 -i $(INDEX_ID) -c -x 4 -y 4 -r 3 -R -F $(TEMP_NOQUADS)
HPQUADS_OPTS  := -l 4 -u 5 -n 4400 -i $(INDEX_ID) -c -x 1 -y 1 -r 6 -R -F $(TEMP_NOQUADS)
QUADLOC_OPTS  := -n 100 -n 1000
CODEPROJ_OPTS := -d
HOMER_OPTS    := -X X -Y Y -d 10 -t 0.25
MATLAB_OPTS   := -nojvm -nosplash -nodisplay

MATLAB_INIT   := addpath('~/quads')

INDEXES := $(addprefix index,$(NUMS2))

.PHONY: all
all: solve1

# A number of our Makefile rules are duplicated for each of the 12
# healpixes.  In order to avoid duplicating code, we put the rules in
# "template" files like "makefile.index-template".  This little script
# substitutes $(1) into the two-digit healpix number: $(1) -> 04, for
# example.  Note that it uses "sed", so it doesn't know anything about
# makefile syntax, so will ruthlessly substitute any "$(1)" it runs across.
# We used to do this with Make's "define ... endef" and "eval(call(...))",
# but this has different bugs in various versions of GNU make - 3.80 and
# 3.81 are inconsistent, for example.
makefile.%-all: makefile.%-template
	-rm -f /tmp/tmp.$@
	for x in $(NUMS2); do \
		$(SED) s/$$\(1\)/$$x/g $< >> /tmp/tmp.$@; \
	done
	mv /tmp/tmp.$@ $@

include makefile.index-all
include makefile.cut-all
include makefile.prospectus-all

# According to
#   http://www.gnu.org/s/h/260/dstn/mysql-5.0.26/var/mysql.socoftware/make/manual/make.html#Chained-Rules
# this should cause make to not try to recreate the ".objs.fits" file just
# because it doesn't exist.
# We want this for the case where people download the skdt.
.SECONDARY: $(CUT_OBJS)
.SECONDARY: $(AN_CATALOG_PRESENT)
#.SECONDARY: $(TYCHO2_FITS_PRESENT)

# The USNO-B and Astrometry.net catalogs contain a large number of files.
# It takes a long time for Make to generate the filenames, check the
# timestamps, and compute the dependencies.  In order to shortcut this
# process, we use intermediate files, named like "usnob/usnob-present"
# to represent the fact that the USNO-B catalog is present.  Then, instead
# of depending directly on the catalog files, targets can depend on the
# "-present" file.  The rules to build the "-present" file (and the files it
# represents) are kept in a separate makefile - for USNO-B, it's called
# "makefile.usnob".  

# This trick is used for:
#   Raw USNO-B             (usnob)
#   USNO-B in FITS format  (usnob-fits)
#   Tycho-2 in FITS        (tycho2-fits)
#   Astrometry.net         (an-catalog)

# Since Tycho-2 is only 3 files (only 2 of which we care about), we don't
# use this trick for them.  Tycho-2-FITS is also only 2 files, but we use
# the trick to make the Astrometry.net rules cleaner: USNO-B and Tycho-2
# look the same this way.

$(USNOB_PRESENT):
	$(MAKE) -f makefile.usnob $@

$(USNOB_FITS_PRESENT):
	$(MAKE) -f makefile.usnob_fits.rules $@

$(TYCHO2_FITS_PRESENT):
	$(MAKE) -f makefile.tycho2_fits $@

$(AN_CATALOG_PRESENT):
	$(MAKE) -f makefile.an_catalog $@

# These are convenience short-form rules:
.PHONY: usnob-fits
usnob-fits: $(USNOB_FITS_PRESENT)

.PHONY: tycho2-fits
tycho2-fits: $(TYCHO2_FITS_PRESENT)

.PHONY: an-catalog
an-catalog: $(AN_CATALOG_PRESENT)

tycho2_download:
	$(MAKE) -f makefile.tycho2 $@

include makefile.prospectus

.PHONY: solve1

solve1: solved/solved1
	@S=`$(PRINTSOLVED) solved/solved1 | $(GREP) -v File | $(WC) -w`; \
	S=$$((S)); \
	T=$(TOTAL_FIELDS); \
	U=$$((T - S)); \
	R=`echo "scale=2; 100.0 * $$S / $$T" | $(BC)`; \
	echo; \
	echo "======================================"; \
	echo "SOLVING COMPLETE"; \
	echo; \
	echo "    FIELDS SOLVED:   $$S"; \
	echo "    FIELDS UNSOLVED: $$U"; \
	echo "    TOTAL FIELDS:    $$T"; \
	echo "    SOLVE RATE:      $$R %"; \
	echo "======================================"; \
	echo

input/input1: create-inputs-1
	-$(MKDIR) input match done solved out err
	-$(RM) input/input1
	./create-inputs-1

solved/solved1: input/input1 $(INDEX02)
	@echo
	@echo
	@echo "You might want to put this job in the background and"
	@echo "watch the grass grow:"
	@echo "    tail -f err/err1"
	@echo
	@echo
	$(TOUCH) timestamp-blind-solve1-start
	$(BLIND) <input/input1 >out/out1 2>err/err1
	$(TOUCH) timestamp-blind-solve1-done
	$(MV) solved/tmp.solved1 solved/solved1
	$(MV)  match/tmp.match1   match/match1

.PHONY: solve2

solve2: solved/solved2
	@S=`$(PRINTSOLVED) solved/solved2 | $(GREP) -v File | $(WC) -w`; \
	S=$$((S)); \
	T=$(TOTAL_FIELDS); \
	U=$$((T - S)); \
	R=`echo "scale=2; 100.0 * $$S / $$T" | $(BC)`; \
	echo; \
	echo "======================================"; \
	echo "SOLVING COMPLETE"; \
	echo; \
	echo "    FIELDS SOLVED:   $$S"; \
	echo "    FIELDS UNSOLVED: $$U"; \
	echo "    TOTAL FIELDS:    $$T"; \
	echo "    SOLVE RATE:      $$R %"; \
	echo "======================================"; \
	echo

input/input2: create-inputs-2
	-$(MKDIR) input match done solved out err
	-$(RM) input/input2
	./create-inputs-2

solved/solved2: done/done1 input/input2 $(INDEX02)
	@echo
	@echo
	@echo "You might want to put this job in the background and"
	@echo "watch the grass grow:"
	@echo "    tail -f err/err2"
	@echo
	@echo
	$(CP) solved/solved1 solved/tmp.solved2
	$(TOUCH) timestamp-blind-solve2-start
	$(BLIND) <input/input2 >out/out2 2>err/err2
	$(TOUCH) timestamp-blind-solve2-done
	$(MV) solved/tmp.solved2 solved/solved2
	$(MV)  match/tmp.match2   match/match2

# Cancel stupid built-in implicit rules.
create-inputs-1: ;
create-inputs-2: ;

makefile.prospectus: ;
makefile.prospectus-template: ;
makefile.cut-template: ;
makefile.index-template: ;
makefile.usnob: ;
makefile.usnob_fits.files: ;
makefile.usnob_fits.rules: ;
makefile.tycho2: ;
makefile.tycho2_fits: ;
makefile.an_catalog: ;
Makefile: ;

endif

