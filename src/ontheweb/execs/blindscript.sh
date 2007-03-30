#! /bin/bash

# input is a full path .../ontheweb-data/JOBID/input
input=$1
inputfile=$(basename $input)
jobdir=$(dirname $input)
jobid=$(basename $jobdir)

#echo $input
#echo $inputfile
#echo $jobdir
#echo $jobid

cd $jobdir
LOG=log
echo "running blind..." >> $LOG
blind < $inputfile >> $LOG 2>&1

echo "doing donescript..." >> $LOG
. donescript

return;

if [ -e solved -a `/home/gmaps/quads/printsolved solved | grep -v "File" | wc -w` -eq 1 ]; then 
  echo "Field solved." >> $LOG;
  echo "Running wcs-xy2rd..." >> $LOG;
  /home/gmaps/quads/wcs-xy2rd -w wcs.fits -i field.xy.fits -o field.rd.fits >> $LOG 2>&1;
  echo "Running rdlsinfo..." >> $LOG;
  /home/gmaps/quads/rdlsinfo field.rd.fits > rdlsinfo 2>> $LOG;
  echo "Merging index rdls file..." >> $LOG;
  cp index.rd.fits index.rd.fits.orig
  /home/gmaps/quads/fitsgetext -e 0 -e 1 -i index.rd.fits -o index.rd.fits.tmp >> $LOG 2>&1
  N=$(( $(/home/gmaps/quads/fitsgetext -i index.rd.fits.orig | wc -l) - 1 ))
  for ((i=2; i<$N; i++)); do
	  /home/gmaps/quads/tabmerge index.rd.fits+$i index.rd.fits.tmp+1 >> $LOG 2>&1
  done
  mv index.rd.fits.tmp index.rd.fits
  /home/gmaps/quads/wcs-rd2xy -w wcs.fits -i index.rd.fits -o index.xy.fits >> $LOG 2>&1;
else
	echo "Field did not solve." >> $LOG;
fi
touch done

