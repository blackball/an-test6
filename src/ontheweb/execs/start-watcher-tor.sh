#! /bin/bash

# Hoo-ah!
Nthreads=4
Cmd="/home/gmaps/an/ontheweb/execs/blindscript-remote-nice.sh %s"

cd /home/gmaps/ontheweb-data/tor
rm queue

Epoch=`pwd`/`date +%Y%m`

/home/gmaps/an/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd" -w $Epoch

