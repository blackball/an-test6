#! /bin/bash

# Use this script to start the "watcher" program, which watches a directory,
# waits for new jobs to arrive, and runs a script on each one.

Nthreads=4
Cmd="/home/gmaps/alpha/ontheweb/execs/blindscript-remote.sh c27nice %s"

cd /home/gmaps/ontheweb-data/alpha
rm queue

# We have to tell it to watch the current epoch's directory; new epochs
# will automatically be watched when their directories appear.
Epoch=`pwd`/`date +%Y%m`

# In case the watcher is created before the current epoch's directory has been
# created, create it now.
mkdir -p $Epoch

# This determines which filenames are handled.
Pattern="/input\$"
echo pattern is $Pattern

/home/gmaps/alpha/ontheweb/execs/watcher -D -p $Pattern -n $Nthreads -c "$Cmd" -w $Epoch

