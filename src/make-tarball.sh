#! /bin/bash

# This file is part of the Astrometry.net suite.
# Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.
#
# The Astrometry.net suite is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, version 2.
#
# The Astrometry.net suite is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Astrometry.net suite ; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#
# Create a code snapshot tarball using "svn export".
#

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

echo
echo
echo "Created tarball: $DIR.tar.gz"
echo
echo
