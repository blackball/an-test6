#! /bin/bash

# Hoo-ah!
Nthreads=8
Cmd="/home/gmaps/ontheweb/execs/blindscript-remote.sh %s"

/home/gmaps/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd"

