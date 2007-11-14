#! /bin/bash

# Use this script to start the "watcher" program, which watches a directory,
# waits for new jobs to arrive, and runs a script on each one.

Nthreads=4
Cmd="/home/gmaps/test/ontheweb/execs/blindscript-remote.sh c27test %s"

cd /home/gmaps/ontheweb-data/test
rm queue

# We have to tell it to watch the current epoch's directory; new epochs
# will automatically be watched when their directories appear.
Epoch=`pwd`/`date +%Y%m`
Pattern="/input\$"
echo pattern is $Pattern
/home/gmaps/test/ontheweb/execs/watcher -D -p $Pattern -n $Nthreads -c "$Cmd" -w $Epoch

