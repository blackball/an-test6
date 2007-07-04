#! /bin/bash

# Use this script to start the "watcher" program, which watches a directory,
# waits for new jobs to arrive, and runs a script on each one.

Nthreads=4
#Cmd="/home/gmaps/an/ontheweb/execs/blindscript-remote-nice.sh %s"
Cmd="/home/gmaps/an-2/ontheweb/execs/blindscript-remote-testing.sh %s"

cd /home/gmaps/ontheweb-data/tor
rm queue

Epoch=`pwd`/`date +%Y%m`

/home/gmaps/an-2/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd" -w $Epoch

