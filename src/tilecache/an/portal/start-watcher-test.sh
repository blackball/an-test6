#! /bin/bash

Nthreads=4
Cmd="/home/gmaps/test/tilecache/an/portal/watcher-script-test.py an_remote_test %s"

cd /home/gmaps/test/job-queue
rm queue

#/home/gmaps/test/ontheweb/execs/watcher -D -n $Nthreads -c "$Cmd"
/home/gmaps/test/ontheweb/execs/watcher -n $Nthreads -c "$Cmd"

