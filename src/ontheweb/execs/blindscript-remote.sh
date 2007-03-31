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

touch start

(echo $jobid; tar cf - field.xy.fits input) | ssh -x -T c27 2>>$LOG | tar xvf - --atime-preserve -m
#> out


echo "running donescript..." >> $LOG
./donescript >> $LOG 2>&1
echo "finished donescript." >> $LOG
