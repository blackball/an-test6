from numpy import *
from pylab import *
from tweak_lib import *
from constants import *
import getopt
import os

# Debug stuff
# folder = 'data/tweaktest1/'
# catalogFilename = folder + 'index.xy.fits'
# imageFilename = folder + 'field.xy.fits'

catalogFilename = ()
imageFilename = ()
outputFilename = ()
warpDegree = ()
progressiveWarp = DEFAULT_PROGRESSIVE_WARP

(opts, args) = getopt.getopt(sys.argv[1:], 'f:i:o:d:p')

for opt in opts:
	if opt[0] == '-f':
		imageFilename = opt[1]
	elif opt[0] == '-i':
		catalogFilename = opt[1]
	elif opt[0] == '-o':
		outputFilename = opt[1]
	elif opt[0] == '-d':
		warpDegree = opt[1]
	elif opt[0] == '-p':
		progressiveWarp = True

if (catalogFilename == ()) | (imageFilename == ()) | (outputFilename == ()):
	print 'invalid arguments'
	print 'usage: python tweak -i index_FITS -f field_FITS -o output_FITS'
	raise SystemExit

if warpDegree == ():
	print 'warp degree not specified, using default of', DEFAULT_WARP_DEGREE
	warpDegree = DEFAULT_WARP_DEGREE
	

catalogData = loadCatalogData(catalogFilename)
(imageData, imageFITS) = loadImageData(imageFilename)

catalogData = trimCatalog2Image(catalogData, imageData)

renderCatalogImage(catalogData, imageData)

if progressiveWarp:
	for deg in arange(1, warpDegree+1):
		imageData = tweakImage(imageData, catalogData, deg)
else:
	imageData = tweakImage(imageData, catalogData, warpDegree)

for i in arange(0, imageFITS[1].data.field('X').shape[0]):
	imageFITS[1].data.field('X')[i] = imageData['x'][i]
	imageFITS[1].data.field('Y')[i] = imageData['y'][i]

if outputFilename != ():
	try:
		print 'deleting existing output file'
		os.remove(outputFilename)
	except:
		junk = 1
	print 'writing warped image to', outputFilename
	imageFITS.writeto(outputFilename)
else:
	print 'no output image specified, not writing warped image'

renderCatalogImage(catalogData, imageData)
show()

