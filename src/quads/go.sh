#!/bin/bash
mkdir -p ./tmp_go_dir
cd tmp_go_dir
rm -rf quads pipeline *.tar.gz
wget -q http://monte.ai.toronto.edu:8080/quads/quads.tar.gz
wget -q http://monte.ai.toronto.edu:8080/quads/cut-02.tar.gz
wget -q http://monte.ai.toronto.edu:8080/quads/sdssfield01-hp02.tar.gz
tar xvzf quads.tar.gz
tar xvzf cut-02.tar.gz
tar xvzf sdssfield01-hp02.tar.gz
(cd quads; make)
cd pipeline
make solve1
