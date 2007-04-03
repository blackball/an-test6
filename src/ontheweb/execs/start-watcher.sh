#! /bin/bash

Nthreads=3
Cmd="/home/gmaps/ontheweb/execs/blindscript.sh %s"

cd /home/gmaps/ontheweb-data
rm queue
/home/gmaps/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd"

