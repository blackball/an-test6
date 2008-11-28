#! /bin/bash

# Use this script to start the "watcher" program, which watches a directory,
# waits for new jobs to arrive, and runs a script on each one.

Nthreads=10
Cmd="/home/gmaps/alpha/ontheweb/execs/blindscript-remote.sh c27 %s"

cd /home/gmaps/ontheweb-data/demo
rm queue

# We have to tell it to watch the current epoch's directory; new epochs
# will automatically be watched when their directories appear.
Epoch=`pwd`/`date +%Y%m`

# In case the watcher is created before the current epoch's directory has been
# created, create it now.
mkdir -p $Epoch

Pattern="/input\$"
echo pattern is $Pattern
/home/gmaps/alpha/ontheweb/execs/watcher -D -p $Pattern -n $Nthreads -c "$Cmd" -w $Epoch

