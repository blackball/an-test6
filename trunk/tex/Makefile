.SUFFIXES: .tex .dvi .ps .pdf .mp .eps

all: quad_statistics.pdf sdss_quads.pdf

sdss_quads.dvi: quad.ps

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
