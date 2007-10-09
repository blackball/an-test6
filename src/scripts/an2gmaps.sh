#! /bin/bash

# Convert an Astrometry.net catalog to merctrees to be used in the Google Maps interface.

# input filename pattern
IN=/nobackup2/dstn/stumm/stars/spike_fits_09_11/firstBatch/an_hp%03i.fits
#IN=/nobackup2/dstn/stumm/stars/testFiles/09_11/an_bad_hp%03i.fits

# output filename pattern
OUT=merc_%02i_%02i.mkdt.fits
#OUT=merc_bad_%02i_%02i.mkdt.fits

# logfile
LOG=log
#LOG=log-bad

# executable
MERCTREE=/u/dstn/an/usnob-map/execs/make-merctree2

# ignore input files that don't exist
MTARGS=-I

for ((i=0; i<32; i++)); do
        for ((j=0; j<32; j++)); do
                n=$((i*32 + j))
                out=`printf $OUT $i $j`
                echo $MERCTREE $MTARGS -o $out -M 32 -N $n -i $IN -H 9 -b >> $LOG 2>&1
                $MERCTREE $MTARGS -o $out -M 32 -N $n -i $IN -H 9 -b >> $LOG 2>&1
        done
done
