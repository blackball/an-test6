comma := ,
empty :=
VER := $(shell make --version | grep "GNU Make version" | awk '{print $$4}')
ifeq ($(VER), $(empty))
  VER := $(shell make --version | grep "GNU Make" | awk '{print $$3}')
endif
VER:=$(subst $(comma),$(empty),$(VER))
$(warning make version $(VER))
ifeq ($(VER), 3.79.1)
  $(error You are running a stupid version of Make.  Try version 3.81)
endif

TYCHO2_DIR := tycho2/
                    # (note, trailing '/' required!)
TYCHO2_FILES := catalog.dat suppl_1.dat
TYCHO2 := $(addprefix $(TYCHO2_DIR),$(TYCHO2_FILES))
TYCHO2_URL := http://www.astro.ku.dk/~cf/CD/data/

USNOB_DIR := usnob/
                  # (note, trailing '/' required!)
USNOB := $(shell for ((i=0; i<180; i++)); do for ((j=0; j<10; j++)); do printf "$(USNOB_DIR)%03i/b%03i%i.cat " $$i $$i $$j; done; done)

TYCHO2_FITS_DIR := tycho2-fits/
                              # (note, trailing '/' required!)
TYCHO2_FITS := $(addprefix $(TYCHO2_FITS_DIR),$(subst .dat,.fits,$(TYCHO2_FILES)))

USNOB_FITS_DIR := usnob-fits/
                            # (note, trailing '/' required!)
USNOB_FITS_TEMPLATE := "$(USNOB_FITS_DIR)usnob10_hp%03i.fits"
USNOB_FITS_NSIDE := 9
USNOB_FITS_HP := $(shell echo $$(($(USNOB_FITS_NSIDE) * $(USNOB_FITS_NSIDE) * 12)))
USNOB_FITS := $(shell for ((i=0; i<$(USNOB_FITS_HP); i++)); do printf "$(USNOB_FITS_TEMPLATE) " $$i; done)

AN_DIR    := an/
               # (note, a trailing '/' is required!)
AN_TEMPLATE := $(AN_DIR)an_hp%03i.fits
AN_NSIDE  := 9
AN_HP     := $(shell echo $$(($(AN_NSIDE) * $(AN_NSIDE) * 12)))
#AN_FORMAT := %03i
AN_FILES  := $(shell for ((i=0; i<$(AN_HP); i++)); do printf "$(AN_TEMPLATE) " $$i; done)

#$(warning USNOB files: $(USNOB))
#$(warning TYCHO-2 files: $(TYCHO2))
#$(warning TYCHO-2 FITS files: $(TYCHO2_FITS))
#$(warning USNOB_FITS_HP is $(USNOB_FITS_HP))
#$(warning USNOB FITS files: $(USNOB_FITS))
#$(warning AN files: $(AN_FILES))

CUT_DIR       := cut/
                    # (note, a trailing '/' is required!)
CUT_BASE      := $(CUT_DIR)an-sdss-
#CUT_TEMPLATE  := $(CUT_DIR)an-sdss-%02i
#CUT_FILES     := $(shell for ((i=0; i<12; i++)); do printf "$(CUT_TEMPLATE) " $$i; done)
CUT_PREFIXES  := $(addprefix $(CUT_PREFIX),$(NUMS2))
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
INDEX_DIR  := sdss-$(INDEX_ID)/
CATALOG    := $(CUT_BASE)
INDEX      := $(INDEX_DIR)sdss-$(INDEX_ID)_
TEMP1      := $(INDEX_DIR)phase1_
TEMP2      := $(INDEX_DIR)phase2_

IDFILES := no

WGET            := wget --continue
MKDIR           := mkdir -p
TOUCH           := touch
MV              := mv

USNOBTOFITS     := usnobtofits
TYCHO2TOFITS    := tycho2tofits
BUILD_AN        := build_an_catalog
CUT_AN          := cut_an
STARTREE        := startree

UNPERMUTE_QUADS := unpermute-quads
UNPERMUTE_STARS := unpermute-stars
CODETREE        := codetree
HPQUADS         := hpquads
QUADIDX			:= quadidx
QUADLOCATIONS 	:= quadlocations 
QUADSPERSTAR 	:= quadsperstar 
CODEPROJ        := codeprojections
MATLAB			:= matlab
PNMTOPNG		:= pnmtopng
EPSTOPNG		:= epstopng

% s=4;
% Nside = sqrt((4*pi) / ((s/60*pi/180).^2) / 12)

CODETREE_OPTS := -R 15
HPQUADS_OPTS  := -l 4 -u 5 -n 700 -i $(INDEX_ID) -c -x 6 -y 6 -r 4
QUADLOC_OPTS  := -n 100 -n 1000
CODEPROJ_OPTS := -d
MATLAB_OPTS   := -nojvm -nosplash -nodisplay

MATLAB_INIT   := addpath('~/quads')

define INDEX_template_A
.PHONY: index$(1)
index$(1) : $(addprefix $(INDEX)$(1), .quad.fits .ckdt.fits .skdt.fits)
endef

NUMS2 := 00 01 02 03 04 05 06 07 08 09 10 11
$(foreach num,$(NUMS2),$(eval $(call INDEX_template_A,$(num))))
INDEXES := $(addsuffix $(NUMS2),index)



#all: $(USNOB_FITS) $(TYCHO2_FITS) $(AN_FILES) $(CUTS) $(INDEXES)

all: $(INDEXES)


$(USNOB):
	@echo
	@echo "The Makefile has decided that you need the original USNO-B1.0 data files."
	@echo
	@echo "The files were expected to be named like this:"
	@echo "     $(firstword $(USNOB))"
	@echo
	@echo "If you have the USNO-B1.0 data files, you might need to set up symlinks"
	@echo "or edit the Makefile (look for the USNOB_DIR variable) to point to the"
	@echo "right place."
	@echo
	@echo "If you don't have the USNO-B1.0 data files, you either need to mail a disk"
	@echo "to the Navy Observatory in Arizona, or talk to the folks at Astrometry.net"
	@echo "to find out about downloading it.  It's 80 GB."
	@echo

$(TYCHO2):
	@echo
	@echo "The Makefile has decided that you need the original Tycho-2 data files."
	@echo
	@echo "The files were expected to be named like this:"
	@echo "    $(TYCHO2)"
	@echo
	@echo "If you have the Tycho-2 data files, you might need to set up symlinks"
	@echo "or edit the Makefile (look for the TYCHO2_DIR variable) to point to the"
	@echo "right place."
	@echo
	@echo "If you don't have the Tycho-2 data files, you can automatically download"
	@echo "them by running:"
	@echo "    make tycho2_download"
	@echo "The files are a little over 500 MB and will be placed in the location"
	@echo "shown above."
	@echo

.PHONY: tycho2_download
tycho2_download:
	$(MKDIR) $(TYCHO2_DIR)
	$(TOUCH) timestamp-tycho2-download-start
	$(WGET) -O $(TYCHO2_DIR)catalog.dat $(TYCHO2_URL)catalog.dat
	$(WGET) -O $(TYCHO2_DIR)suppl_1.dat $(TYCHO2_URL)suppl_1.dat
	$(TOUCH) timestamp-tycho2-download-done

.PHONY: usnob_fits
usnob_fits: $(USNOB_FITS)
$(USNOB_FITS): $(USNOB)
	$(MKDIR) $(USNOB_FITS_DIR)
	$(TOUCH) timestamp-usnobtofits-start
	$(USNOBTOFITS) -o $(USNOB_FITS_TEMPLATE) -N $(USNOB_FITS_NSIDE) $(USNOB)
	$(TOUCH) timestamp-usnobtofits-done

.PHONY: tycho2_fits
tycho2_fits: $(TYCHO2_FITS)
$(TYCHO2_FITS):: %.fits : %.dat
	$(MKDIR) $(TYCHO2_FITS_DIR)
	$(TOUCH) timestamp-tycho2tofits-start
	$(TYCHO2TOFITS) -o $@ $<
	$(TOUCH) timestamp-tycho2tofits-done

.PHONY: an_catalog
an_catalog: $(AN_FILES)
$(AN_FILES): $(USNOB_FITS) $(TYCHO2_FITS)
	$(MKDIR) $(AN_DIR)
	$(TOUCH) timestamp-build-an-cat-start
	$(BUILD_AN) -o $(AN_FILE) -N $(AN_NSIDE) $(USNOB_FITS) $(TYCHO2_FITS)
	$(TOUCH) timestamp-build-an-cat-done

define CUT_template
.PHONY: cut$(1)
$(CUT_BASE)$(1).skdt.fits: $(CUT_BASE)$(1).objs.fits
	$(MKDIR) $(CUT_DIR)
	$(TOUCH) timestamp-cut-startree-$(1)-start
	$(STARTREE) -f $(CUT_BASE)$(1) -R $(CUT_NLEAF)
	$(TOUCH) timestamp-cut-startree-$(1)-done

cut$(1): $(CUT_BASE)$(1).objs.fits

$(CUT_BASE)$(1).objs.fits: $(AN_FILES)
	$(MKDIR) $(CUT_DIR)
	$(TOUCH) timestamp-cut-an-$(1)-start
	$(CUT_AN) -H $(1) -o $(CUT_BASE)$(1).objs.fits -i $(CUT_BASE)$(1).id.fits \
		-N $(CUT_AN_NSIDE) -n $(CUT_AN_SWEEPS) -d $(CUT_AN_DEDUP) $(CUT_AN_CUT) \
		`hpowned -f $(AN_TEMPLATE) -m -N $(AN_NSIDE) $(1)`
	$(TOUCH) timestamp-cut-an-$(1)-done
endef
$(foreach num,$(NUMS2),$(eval $(call CUT_template,$(num))))

define INDEX_template_B
$(INDEX)$(1).quad.fits $(INDEX)$(1).ckdt.fits : \
		$(TEMP2)$(1).ckdt.fits $(TEMP2)$(1).quad.fits
	$(UNPERMUTE_QUADS) -f $(TEMP2)$(1) -o tmp.$(INDEX)$(1)
	$(MV) tmp.$(INDEX)$(1).ckdt.fits $(INDEX)$(1).ckdt.fits
	$(MV) tmp.$(INDEX)$(1).quad.fits $(INDEX)$(1).quad.fits

ifeq ($(IDFILES), yes)
$(INDEX)$(1).skdt.fits $(TEMP2)$(1).quad.fits $(INDEX)$(1).id.fits : \
		$(CATALOG)$(1).skdt.fits $(CATALOG)$(1).id.fits $(TEMP1)$(1).quad.fits
	$(UNPERMUTE_STARS) -f $(CATALOG)$(1) -q $(TEMP1)$(1) -o tmp.$(INDEX)$(1)
	$(MV) tmp.$(INDEX)$(1).skdt.fits $(INDEX)$(1).skdt.fits
	$(MV) tmp.$(INDEX)$(1).id.fits   $(INDEX)$(1).id.fits
	$(MV) tmp.$(INDEX)$(1).quad.fits $(TEMP2)$(1).quad.fits
else
$(INDEX)$(1).skdt.fits $(TEMP2)$(1).quad.fits : \
		$(CATALOG)$(1).skdt.fits $(TEMP1)$(1).quad.fits
	$(UNPERMUTE_STARS) -f $(CATALOG)$(1) -q $(TEMP1)$(1) -o tmp.$(INDEX)$(1)
	$(MV) tmp.$(INDEX)$(1).skdt.fits $(INDEX)$(1).skdt.fits
	$(MV) tmp.$(INDEX)$(1).quad.fits $(TEMP2)$(1).quad.fits
endif

$(TEMP2)$(1).ckdt.fits: $(TEMP1)$(1).code.fits
	$(CODETREE) $(CODETREE_OPTS) -f $(TEMP1)$(1) -o tmp.$(TEMP2)$(1)
	$(MV) tmp.$(TEMP2)$(1).ckdt.fits $(TEMP2)$(1).ckdt.fits

$(TEMP1)$(1).quad.fits $(TEMP1)$(1).code.fits : $(CATALOG)$(1).skdt.fits
	$(HPQUADS) $(HPQUADS_OPTS) -f $(CATALOG)$(1) -o tmp.$(TEMP1)$(1)
	$(MV) tmp.$(TEMP1)$(1).quad.fits $(TEMP1)$(1).quad.fits
	$(MV) tmp.$(TEMP1)$(1).code.fits $(TEMP1)$(1).code.fits

SEC += \
    $(addprefix $(TEMP1)$(1), .code.fits .quad.fits) \
    $(addprefix $(TEMP2)$(1), .ckdt.fits .quad.fits)
endef

define PROSPECTUS_template
$(INDEX)$(1).qidx.fits: $(INDEX)$(1).quad.fits
	$(QUADIDX) -f $(INDEX)$(1)

PHO += prospectus$(1)
prospectus$(1): ana$(1) plot$(1) dplot$(1) qps$(1)

quad$(1).pgm quad$(1)_small.pgm: $(INDEX)$(1).skdt.fits $(INDEX)$(1).quad.fits
	$(QUADLOCATIONS) $(QUADLOC_OPTS) -o quad$(1)_small.pgm -o quad$(1).pgm $(INDEX)$(1)

proj$(1).m: $(INDEX)$(1).ckdt.fits
	$(CODEPROJ) $(CODEPROJ_OPTS) -F $$< > $$@

PHO += ana$(1)
ana$(1): quad$(1).png quad$(1)_small.png proj$(1).m

PHO += plot$(1)
plot$(1): cp_2dhists$(1).png cp_2dhists$(1)_small.png \
	cp_1dhists$(1).png cp_1dhists$(1)_small.png \
	cp_xy$(1).png cp_xy$(1)_small.png \

PHO += dplot$(1)
dplot$(1): cp_2ddhists$(1).png cp_2ddhists$(1)_small.png \
	cp_1ddhists$(1).png cp_1ddhists$(1)_small.png \
	cp_dxy$(1).png cp_dxy$(1)_small.png

cp_2dhists$(1).eps cp_2ddhists$(1).eps cp_1dhists$(1).eps cp_xy$(1).eps: proj$(1).m
	echo "$(MATLAB_INIT); fprintf('\nGenerating plots for $(1)...\n'); proj$(1); " \
		"print_plots=1; plot_suffix='$(1)'; batch=1; codeprojections; quit" \
		> /tmp/tmpscript$(1).m
	$(MATLAB) $(MATLAB_OPTS) < /tmp/tmpscript$(1).m

PHO += qps$(1)
qps$(1): qps_$(1).png qps_$(1)_small.png

qps_$(1).eps: qps$(1).m
	echo "$(MATLAB_INIT);" \
		"fprintf('\nGenerating quads per star histograms...\n');" \
		"qps$(1);" \
		"x=$(INDEX_UNDERSCORE)$(1);" \
		"bar([0:length(x)-1],x,1);" \
		"print('-depsc', '$$@');" \
		"quit" > /tmp/tmpscript.m
	$(MATLAB) $(MATLAB_OPTS) < /tmp/tmpscript.m

qps$(1).m: $(INDEX)$(1).qidx.fits
	$(QUADSPERSTAR) $(INDEX)$(1) | sed s/-/_/ > $$@

%$(1)_small.png: %$(1).png
	pngtopnm -- $$< | pamscale -linear 0.2 | pnmtopng > $$@
endef

%.png: %.pgm
	$(PNMTOPNG) -- $< > $@

%.png: %.eps
	$(EPSTOPNG) $< $@

PHO := $(NUMS)
SEC := 

INDEX_UNDERSCORE := $(shell echo $(INDEX) | sed s/-/_/)

$(foreach num,$(NUMS2),$(eval $(call INDEX_template_B,$(num))))
$(foreach num,$(NUMS2),$(eval $(call PROSPECTUS_template,$(num))))

.PHONY: $(PHO)
.SECONDARY: $(SEC)

SUMPLOTS_EPS := cp_2dhists_sum.eps cp_2ddhists_sum.eps \
	cp_1dhists_sum.eps cp_1ddhists_sum.eps \
	cp_xy_sum.eps cp_dxy_sum.eps
$(SUMPLOTS_EPS): $(addsuffix .m, $(addprefix proj, $(NUMS)))
	echo "$(MATLAB_INIT); fprintf('\nGenerating sum plots...\n'); sumprojs; print_plots=1; plot_suffix='_sum'; batch=1; codeprojections; quit" > /tmp/tmpscript$(1).m
	$(MATLAB) $(MATLAB_OPTS) < /tmp/tmpscript$(1).m

SUMPLOTS_PNG := $(SUMPLOTS_EPS:.eps=.png)
SUMPLOTS_SMALL := $(SUMPLOTS_PNG:.png=_small.png)

.PHONY: prospectus_all
prospectus_all: quadsall.png quadsall_small.png qps_sum.png qps_sum_small.png \
	$(SUMPLOTS_PNG) $(SUMPLOTS_SMALL)

.PHONY: prospectus
prospectus: prospectus.html prospectus_all $(addprefix prospectus, $(NUMS))

prospectus.html: prospectus.html.in
	genprospectus $(INDEX) prospectus.html.in > $@

quadsall.pgm quadsall_small.pgm: \
	$(addsuffix .quad.fits, $(addprefix $(INDEX), $(NUMS))) \
	$(addsuffix .skdt.fits, $(addprefix $(INDEX), $(NUMS)))
	$(QUADLOCATIONS) $(QUADLOC_OPTS) $(addprefix $(INDEX), $(NUMS)) \
		-o quadsall_small.pgm -o quadsall.pgm

qps_sum.eps: $(addsuffix .m, $(addprefix qps, $(NUMS)))
	echo "$(MATLAB_INIT); qpssum=[]; for x=0:11, " \
		"eval(sprintf('qps%02i', x));" \
		"s=eval(sprintf('$(INDEX_UNDERSCORE)%02i', x));" \
		"newsum=zeros(1,max(length(qpssum), length(s)));" \
		"if length(qpssum), newsum(1:length(qpssum))=newsum(1:length(qpssum)) + qpssum; end,"\
		"newsum(1:length(s))=newsum(1:length(s)) + s;"\
		"qpssum = newsum;" \
		"end," \
		"bar([0:length(qpssum)-1], qpssum, 1);" \
		"print('-depsc', '$@'); quit" > /tmp/tmpscript.m
	$(MATLAB) $(MATLAB_OPTS) < /tmp/tmpscript.m

$(SUMPLOTS_SMALL) $(QPS_SMALL) qps_sum_small.png :: %_small.png: %.png
	pngtopnm -- $< | pamscale -linear 0.2 | pnmtopng > $@

