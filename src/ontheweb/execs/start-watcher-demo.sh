#! /bin/bash

# Hoo-ah!
Nthreads=10
Cmd="/home/gmaps/an/ontheweb/execs/blindscript-remote.sh %s"

cd /home/gmaps/ontheweb-data/demo
rm queue

Epoch=`pwd`/`date +%Y%m`

/home/gmaps/an/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd" -w $Epoch

