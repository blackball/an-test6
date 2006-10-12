#!/bin/bash
mkdir -p ./tmp_go_dir
cd tmp_go_dir
\rm -rf quads* *.tar.gz
wget -q http://monte.ai.toronto.edu:8080/quads/quads-2230.tar.gz
wget -q http://monte.ai.toronto.edu:8080/quads/cut-02.tar.gz
wget -q http://monte.ai.toronto.edu:8080/quads/sdssfield01-hp02.tar.gz
tar xvzf quads-2230.tar.gz
tar xvzf cut-02.tar.gz -C quads-2230/data
tar xvzf sdssfield01-hp02.tar.gz -C quads-2230/data
cd quads-2230
make
cd data
make solve1
