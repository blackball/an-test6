#! /bin/bash

# usnobtofits currently writes the AN_DIFFRACTION_SPIKE flag.  I want to
# trim it off, before eventually re-adding it after the clean-usnob process
# finishes.

IN=/nobackup2/dstn/USNOB-FITS/
OUT=/nobackup1/roweis/stars/USNOB-FITS/

for x in ${IN}*.fits; do
  echo fitscopy $x"[col -AN_DIFFRACTION_SPIKE]" ${OUT}`basename $x`
  fitscopy $x"[col -AN_DIFFRACTION_SPIKE]" ${OUT}`basename $x`
done
