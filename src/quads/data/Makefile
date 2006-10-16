ifndef MAKEFILE_INCLUDED
MAKEFILE_INCLUDED := 1
export

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

CUT_DIR       := cut/
                    # (note, a trailing '/' is required!)
CUT_BASE      := $(CUT_DIR)an-$(INAME)-
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
TEMP_DIR   := 
CATALOG    := $(CUT_BASE)
INDEX      := $(INDEX_DIR)$(INAME)-$(INDEX_ID)-
PHASE1      := $(INDEX_DIR)phase1-
PHASE2      := $(INDEX_DIR)phase2-
TEMP1      := $(TEMP_DIR)tmp.phase1-
TEMP2      := $(TEMP_DIR)tmp.phase2-
TEMPINDEX  := $(TEMP_DIR)tmp.$(INAME)-$(INDEX_ID)-

SDSS_FIELDS := sdss-fields/
SDSS_HOMER  := sdss-fields-homer/

WGET            := wget --continue
MKDIR           := mkdir -p
TOUCH           := touch
MV              := mv
SED             := sed

AN_DIR          := ../

USNOBTOFITS     := $(AN_DIR)usnobtofits
TYCHO2TOFITS    := $(AN_DIR)tycho2tofits
HPOWNED         := $(AN_DIR)hpowned
BUILD_AN        := $(AN_DIR)build-an-catalog
CUT_AN          := $(AN_DIR)cut-an
STARTREE        := $(AN_DIR)startree
UNPERMUTE_QUADS := $(AN_DIR)unpermute-quads
UNPERMUTE_STARS := $(AN_DIR)unpermute-stars
CODETREE        := $(AN_DIR)codetree
HPQUADS         := $(AN_DIR)hpquads

HOMER           := $(AN_DIR)homer
SLAVE           := $(AN_DIR)slave

QUADIDX			:= $(AN_DIR)quadidx
QUADLOCATIONS 	:= $(AN_DIR)quadlocations 
QUADSPERSTAR 	:= $(AN_DIR)quadsperstar 
CODEPROJ        := $(AN_DIR)codeprojections
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
HOMER_OPTS    := -X ROWC -Y COLC -d 10 -t 0.25
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

$(USNOB_PRESENT):
	$(MAKE) -f makefile.usnob $@

$(USNOB_FITS_PRESENT):
	$(MAKE) -f makefile.usnob_fits $@

.PHONY: usnob-fits
usnob-fits: $(USNOB_FITS_PRESENT)

#$(TYCHO2_PRESENT):
#	$(MAKE) -f makefile.tycho2 $@

$(TYCHO2_FITS_PRESENT):
	$(MAKE) -f makefile.tycho2_fits $@

.PHONY: tycho2-fits
tycho2-fits: $(TYCHO2_FITS_PRESENT)

.PHONY: an-catalog
an-catalog: $(AN_CATALOG_PRESENT)

$(AN_CATALOG_PRESENT):
	$(MAKE) -f makefile.an_catalog $@

tycho2_download:
	$(MAKE) -f makefile.tycho2 $@

include makefile.prospectus

#include makefile.homer

solve1: index02
	-$(MKDIR) input match done solved out err
	-$(RM) input/input-*
	./create-inputs-1
	@echo
	@echo
	@echo "You might want to put this job in the background and"
	@echo "watch the grass grow:"
	@echo "    tail -f err/err-000 | grep Field   "
	@echo
	@echo
	touch timestamp-slave-solve1-start
	$(SLAVE) <input/input.000 >out/out-000 2>err/err-000
	touch timestamp-slave-solve1-done

endif

