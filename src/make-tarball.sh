#! /bin/bash

DATE=`date "+%Y-%m-%d"`
oDIR=astrometry.net-snapshot-$DATE
SVN="svn+ssh://astrometry.net/svn/trunk/src"

echo "Checking out code in directory $DIR..."

cmd="svn export -N $SVN $DIR"
echo $cmd
$cmd || exit

for x in cfitsio qfits-an an-common libkd tweak quads simplexy; do
    cmd="svn export $SVN/$x $DIR/$x"
    echo $cmd
    $cmd || exit
done

cmd="tar czf $DIR.tar.gz $DIR"
echo $cmd
$cmd || exit

