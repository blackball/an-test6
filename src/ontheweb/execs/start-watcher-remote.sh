#! /bin/bash

# Hoo-ah!
Nthreads=10
Cmd="/home/gmaps/ontheweb/execs/blindscript-remote.sh %s"

cd /home/gmaps/ontheweb-data
rm queue
/home/gmaps/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd"

