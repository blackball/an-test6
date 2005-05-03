.SUFFIXES: .tex .dvi .ps .pdf .mp .eps

all: xymatch.ps.gz

%.pdf: %.ps
	ps2pdf -dMaxSubsetPct=100 -dCompatibilityLevel=1.2 -dSubsetFonts=true -dEmbedAllFonts=true $<

%.gz: %
	gzip -f $<

%.ps: %.dvi
	dvips -t letter $< -o

%.dvi: %.tex %.bbl
	latex $<
	latex $<
	latex $<
