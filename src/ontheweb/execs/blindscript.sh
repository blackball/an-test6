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

echo "running donescript..." >> $LOG
. donescript
echo "finished donescript." >> $LOG
