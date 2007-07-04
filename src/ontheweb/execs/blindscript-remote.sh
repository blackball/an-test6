#! /bin/bash

if [ $# -ne 2 ]; then
	echo "Usage: $0 <ssh-config> <input-file>"
	exit
fi

# This script gets run by "watcher" when we are running in remote-compute-server mode.
# It bundles up the input files, sends them over an ssh pipe, then extracts the result
# files when the remote "blind" process is done.

# input is a full path: .../ontheweb-data/SITE/EPOCH/NUM/input
input=$2

sshconfig=$1

inputfile=$(basename $input)
jobdir=$(dirname $input)
numdir=$(dirname $input)
num=$(basename $numdir)
epochdir=$(dirname $numdir)
epoch=$(basename $epochdir)
sitedir=$(dirname $epochdir)
site=$(basename $sitedir)

jobid=$site-$epoch-$num

cd $jobdir
LOG=log

echo "running startscript..." >> $LOG
./startscript >> $LOG 2>&1
echo "finished startscript." >> $LOG

echo "running blind..." >> $LOG
(echo $jobid; tar cf - --ignore-failed-read field.xy.fits input verify.fits) | ssh -x -T $sshconfig 2>>$LOG | tar xf - --atime-preserve -m --exclude=input >>$LOG 2>&1
echo "finished blind." >> $LOG

echo "running donescript..." >> $LOG
./donescript >> $LOG 2>&1
echo "finished donescript." >> $LOG
