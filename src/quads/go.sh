#!/bin/bash
mkdir -p ./tmp_go_dir
cd tmp_go_dir
echo "Removing old files..."
rm -rf quads pipeline *.tar.gz
echo "Downloading source..."
wget http://monte.ai.toronto.edu:8080/quads/quads.tar.gz
echo "Downloading data..."
wget http://monte.ai.toronto.edu:8080/quads/cut-02.tar.gz
echo "Downloading fields..."
wget http://monte.ai.toronto.edu:8080/quads/sdssfield01-hp02.tar.gz
echo "Extracting source..."
tar xvzf quads.tar.gz
echo "Extracting data..."
tar xvzf cut-02.tar.gz
echo "Extracting fields..."
tar xvzf sdssfield01-hp02.tar.gz
echo "Building source..."
(cd quads; make)
echo "Running..."
cd pipeline
make solve1
