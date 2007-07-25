# This file is part of the Astrometry.net suite.
# Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.
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


# Top-level Makefile: just go into the subdirs and make there.

all:
	$(MAKE) -C an-common
	$(MAKE) -C quads

# Targets that require extra libraries or which are otherwise tricky.
extra:
	$(MAKE) -C quads cairo
	$(MAKE) -C usnob-map/execs
	$(MAKE) -C ontheweb/execs

clean:
	$(MAKE) -C an-common clean
	$(MAKE) -C cfitsio clean
	$(MAKE) -C qfits-an clean
	$(MAKE) -C gsl-an clean
	$(MAKE) -C libkd clean
	$(MAKE) -C quads clean
	$(MAKE) -C tweak clean

TAGS:
	etags -I `find . -name "*.c" -o -name "*.h"`

tags:
	ctags-exuberant --fields=+aiKS --c++-kinds=+p --extra=+q -I --file-scope=no -R *
