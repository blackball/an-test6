#! /usr/bin/env python
import os
import sys
import logging

if __name__ == '__main__':
	try:
		import pyfits
		import astrometry
		from astrometry.util.shell import shell_escape
		from astrometry.util.filetype import filetype_short
	except ImportError:
		me = sys.argv[0]
		#print 'i am', me
		path = os.path.realpath(me)
		#print 'my real path is', path
		utildir = os.path.dirname(path)
		assert(os.path.basename(utildir) == 'util')
		andir = os.path.dirname(utildir)
		#assert(os.path.basename(andir) == 'astrometry')
		rootdir = os.path.dirname(andir)
		#print 'adding path', rootdir
		#sys.path += [rootdir]
		sys.path.insert(1, andir)
		sys.path.insert(2, rootdir)

import numpy
import pyfits
from numpy import *
from numpy.random import rand

# Returns a numpy array of booleans
def hist_remove_lines(x, binwidth, binoffset, logcut):
    bins = -binoffset + arange(0, max(x)+binwidth, binwidth)
    (counts, thebins) = histogram(x, bins)

    # We're ignoring empty bins.
    occupied = nonzero(counts > 0)[0]
    noccupied = len(occupied)
    k = (counts[occupied] - 1) 
    mean = sum(k) / float(noccupied)
    logpoisson = k*log(mean) - mean - array([sum(arange(kk)) for kk in k])
    badbins = occupied[logpoisson < logcut]
    if len(badbins) == 0:
        return array([True] * len(x))

    badleft = bins[badbins]
    badright = badleft + binwidth

    badpoints = sum(array([(x >= L)*(x < R) for (L,R) in zip(badleft, badright)]), 0)
    return (badpoints == 0)

def removelines(infile, outfile, **kwargs):
    p = pyfits.open(infile)
    xy = p[1].data
    hdr = p[1].header
    x = xy.field('X')
    y = xy.field('Y')

    ix = hist_remove_lines(x, 1, 0.5, -100)
    iy = hist_remove_lines(y, 1, 0.5, -100)
    I = ix * iy
    xc = x[I]
    yc = y[I]
    print 'removelines.py: Removed %i sources' % (len(x) - len(xc))

    p[1].header.add_history('This xylist was filtered by the "removelines.py" program')
    p[1].header.add_history('to remove horizontal and vertical lines of sources')
    p[1].header.update('REMLINEN', len(x) - len(xc), 'Number of sources removed by "removelines.py"')

    p[1].data = p[1].data[I]
    p.writeto(outfile, clobber=True)

    return 0

if __name__ == '__main__':
    if (len(sys.argv) == 3):
        infile = sys.argv[1]
        outfile = sys.argv[2]
        rtncode = removelines(infile, outfile)
        sys.exit(rtncode)
    else:
        print 'Usage: %s <input-file> <output-file>' % sys.args[0]

