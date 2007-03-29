#! /bin/bash

Nthreads=3
Cmd="/home/gmaps/ontheweb/execs/blindscript.sh %s"

/home/gmaps/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd"

