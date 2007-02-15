#! /bin/bash

# This will be replaced by the PHP script.
job=##JOB##
fits2xy_tarball=##FITS2XY_TARBALL##

echo 'Hello World!'
echo "I am job number $job"

echo "Downloading fits2xy source..."

#wget -nc -q "$fits2xy_tarball"

wget -q -O - "$fits2xy_tarball" | tar xz

echo "Building fits2xy..."

(cd fits2xy && make) > build-fits2xy.log 2>&1

echo "Finding FITS images..."

#IMGS=`find . -name "*.fits" -type f -ls`

#IFS=$'\0'
#IMGS=`find . -name "*.fits" -type f -printf "\"%p\"\0"`
#for x in $IMGS; do
#	echo $x;
#done

# Hmm, it seems you can't use "\0" as the IFS...

#export IFS=$'\n'
#export IFS='\n'
IFS=$'\n'
i=0;
for x in $(find . -name "*.fits" -type f -printf "%p\n" 2>/dev/null); do
	echo $x;
	i=$(($i+1));
	ln -s "$x" image$i.fits;
	echo "Processing image $x...";
	./fits2xy/fits2xy image$i.fits;
done


