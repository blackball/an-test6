#SDSS_IM := fpC-000756-r4-0387.fit
#SDSS_IM_URL := http://das.sdss.org/dr5/data/imaging/756/44/corr/4/fpC-000756-r4-0387.fit.gz

SDSS_IM := fpC-003964-r4-0086.fit
SDSS_IM_URL := http://das.sdss.org/dr5/data/imaging/3964/41/corr/4/fpC-003964-r4-0086.fit.gz

all: $(SDSS_IM) ascii.xy.fits ascii.done

$(SDSS_IM).gz:
	wget -c $(SDSS_IM_URL) -O $@

%: %.gz
	gunzip -cd $< > $@

ascii.cat: $(SDSS_IM) ascii.conf ascii.param
	sex -c ascii.conf $(SDSS_IM)

ascii.xy: ascii.cat
	grep -v "#" $< | awk '{print($$3,$$1,$$2)}' | sort -n -r | awk '{print($$2,$$3)}' > $@

ascii.xy.fits: ascii.xy
	xylist2fits -c $< $@

ascii.done: ascii.input ascii.xy.fits
	slave < ascii.input

