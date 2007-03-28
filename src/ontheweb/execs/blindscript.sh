#! /bin/bash

# input is a full path .../ontheweb-data/JOBID/input
input=$1
inputfile=$(basename $input)
jobdir=$(dirname $input)
jobid=$(basename $jobdir)

echo $input
echo $inputfile
echo $jobdir
echo $jobid

cd $jobdir
blind < $inputfile

