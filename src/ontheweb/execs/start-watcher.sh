#! /bin/bash

Nthreads=3
Cmd="/home/gmaps/ontheweb/execs/blindscript.sh %s"

watcher -D -n $Nthreads -c "$Cmd"

