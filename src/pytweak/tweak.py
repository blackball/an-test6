from numpy import *
from pylab import *
from tweak_lib import *
from constants import *
import getopt
import os
import util.sip as sip

catalogXYFilename = ()
catalogRDFilename = ()
WCSFilename = ()
imageFilename = ()
outputImageFilename = ()
outputWCSFilename = ()
warpDegree = ()
progressiveWarp = DEFAULT_PROGRESSIVE_WARP

# Debug stuff
folder = 'data/tweaktest4/'
catalogXYFilename = folder + 'index.xy.fits'
# catalogRDFilename = folder + 'index.rd.fits'
# WCSFilename = folder + 'wcs.fits'
imageFilename = folder + 'field.xy.fits'

(opts, args) = getopt.getopt(sys.argv[1:], 'w:r:x:i:o:s:d:p:')

for opt in opts:
	if opt[0] == '-w':
		WCSFilename = opt[1]
	elif opt[0] == '-f':
		imageFilename = opt[1]
	elif opt[0] == '-x':
		catalogXYFilename = opt[1]
	elif opt[0] == '-r':
		catalogRDFilename = opt[1]
	elif opt[0] == '-o':
		outputImageFilename = opt[1]
	elif opt[0] == '-s':
		outputSIPFilename = opt[1]
	elif opt[0] == '-d':
		warpDegree = opt[1]
	elif opt[0] == '-p':
		progressiveWarp = True

if not((imageFilename != ()) & ( (catalogXYFilename != ()) | ((catalogRDFilename != ()) & (WCSFilename != ())))):
	print 'insufficient input arguments'
	print 'usage: python tweak -x index_XY_FITS OR (-w WCS_FITS -r index_RD_FITS) -f field_FITS [-o output_FITS -p]'
	raise SystemExit

if (catalogXYFilename != ()) & ((catalogRDFilename != ()) | (WCSFilename != ())):
	print 'redundant catalog information, pass either:'
	print ' 1) catalogXY'
	print ' 2) catalogRD & WCS'
	print 'not both'
	raise SystemExit

if warpDegree == ():
	print 'warp degree not specified, using default of', DEFAULT_WARP_DEGREE
	warpDegree = DEFAULT_WARP_DEGREE

if catalogXYFilename != ():
	catalogData = loadCatalogXYData(catalogXYFilename)
else:
	WCS = sip.Sip(WCSFilename)
	catalogRDData = loadCatalogRDData(catalogRDFilename)
	catalogData = {}
	(catalogData['X'], catalogData['Y']) = WCS_ra2xy(WCS, catalogRDData['RA'], catalogRDData['DEC'])

catalogData['SIGMA_X'] = 0*catalogData['X'] + 1.0
catalogData['SIGMA_Y'] = 0*catalogData['X'] + 1.0

(imageData, imageFITS) = loadImageData(imageFilename)

catalogData = trimCatalog2Image(catalogData, imageData)

renderCatalogImage(catalogData, imageData)

if progressiveWarp:
	for deg in arange(1, warpDegree+1):
		imageData = tweakImage(imageData, catalogData, deg)
else:
	imageData = tweakImage(imageData, catalogData, warpDegree)

for i in arange(0, imageFITS[1].data.field('X').shape[0]):
	imageFITS[1].data.field('X')[i] = imageData['X'][i]
	imageFITS[1].data.field('Y')[i] = imageData['X'][i]

if outputImageFilename != ():
	try:
		os.remove(outputImageFilename)
		print 'deleted existing output image file'
	except:
		junk = 1
	print 'writing warped image to', outputImageFilename
	imageFITS.writeto(outputImageFilename)
else:
	print 'no output image specified, warped image not written'

# Gotta write the new WCS here, too

renderCatalogImage(catalogData, imageData)
show()

