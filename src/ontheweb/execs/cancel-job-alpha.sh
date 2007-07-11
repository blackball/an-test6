#! /bin/bash

# Base job directory
BASE="$1"
# Relative job directory
JOB="$2"

# Touch local "cancel" file
cd $BASE && cd $JOB && touch cancel

# Touch remote "cancel" file
echo $JOB | ssh -T c27cancel

