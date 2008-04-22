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
catalogRDFilename = folder + 'index.rd.fits'
WCSFilename = folder + 'wcs.fits'
imageFilename = folder + 'field.xy.fits'

(opts, args) = getopt.getopt(sys.argv[1:], 'w:i:f:o:s:d:p:')

for opt in opts:
	if opt[0] == '-w':
		WCSFilename = opt[1]
	elif opt[0] == '-f':
		imageFilename = opt[1]
	elif opt[0] == '-i':
		catalogRDFilename = opt[1]
	elif opt[0] == '-o':
		outputImageFilename = opt[1]
	elif opt[0] == '-s':
		outputSIPFilename = opt[1]
	elif opt[0] == '-d':
		warpDegree = opt[1]
	elif opt[0] == '-p':
		progressiveWarp = True

if not((imageFilename != ()) & (catalogRDFilename != ()) & (WCSFilename != ())):
	print 'insufficient input arguments'
	print 'usage: python tweak -w WCS_FITS -i index_RD_FITS -f field_FITS [-o output_FITS -s output_WCS -p]'
	raise SystemExit

# if (catalogXYFilename != ()) & ((catalogRDFilename != ()) | (WCSFilename != ())):
# 	print 'redundant catalog information, pass either:'
# 	print ' 1) catalogXY'
# 	print ' 2) catalogRD & WCS'
# 	print 'not both'
# 	raise SystemExit

if warpDegree == ():
	print 'warp degree not specified, using default of', DEFAULT_WARP_DEGREE
	warpDegree = DEFAULT_WARP_DEGREE

(imageData, imageFITS) = loadImageData(imageFilename)

WCS = sip.Sip(WCSFilename)

goalCRPix = array([WCS.wcstan.imagew/2 + 0.5, WCS.wcstan.imageh/2 + 0.5])
#  We add the 0.5 to account for the pixel representation, where center = 1.

catalogData = loadCatalogRDData(catalogRDFilename)

(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
catalogData = trimCatalog2Image(catalogData, imageData)
(catalogData['RA'], catalogData['DEC']) = WCS_xy2rd(WCS, catalogData['X'], catalogData['Y'])

catalogData['SIGMA_X'] = 0*catalogData['X'] + 1.0
catalogData['SIGMA_Y'] = 0*catalogData['X'] + 1.0

renderCatalogImage(catalogData, imageData, WCS)

iter = 0
while True:
	iter = iter + 1
	imageData = tweakImage(imageData, catalogData, 1)

	# shiftSatisfied = False
	# linearWarpSatisfied = False
	# 
	# centerShift = imageData['warpM'][-2:]
	# centerShiftDist = double(sqrt(centerShift.T*centerShift))
	# print 'tangent point shift of ', centerShiftDist
	# 
	# if centerShiftDist > 10**-8:
	# 	newCRPix = (WCS.wcstan.crpix[0] - double(centerShift[0]), WCS.wcstan.crpix[1] - double(centerShift[1]))
	# 	WCS.wcstan.crpix[0] = newCRPix[0]
	# 	WCS.wcstan.crpix[1] = newCRPix[1]
	# 
	# 	newCRVal = WCS.pixelxy2radec(goalCRPix[0], goalCRPix[1])
	# 	WCS.wcstan.crval[0] = newCRVal[0]
	# 	WCS.wcstan.crval[1] = newCRVal[1]
	# 	WCS.wcstan.crpix[0] = goalCRPix[0]
	# 	WCS.wcstan.crpix[1] = goalCRPix[1]
	# 	
	# 	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
	# 	
	# 	imageData = polyWarp(imageData, catalogData, warpDegree)
	# else:
	# 	shiftSatisfied = True
	# 
	# linearWarp = imageData['warpM'][0:-2].reshape(2,-1)[:,-2::]
	# linearWarpAmount = abs(1-linalg.det(linearWarp))
	# print 'linear warp amount of ', linearWarpAmount
	# 
	# if linearWarpAmount > 10**-8:
	# 	CD = matrix(WCS.wcstan.cd[:]).reshape(2,2)
	# 	CD2 = (CD*linearWarp).reshape(-1,1)
	# 	for i in arange(0,4):
	# 		WCS.wcstan.cd[i] = CD2[i]
	# 
	# 	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
	# 	imageData = polyWarp(imageData, catalogData, warpDegree)
	# else:
	# 	linearWarpSatisfied = True
	# 
	# if (shiftSatisfied & linearWarpSatisfied) | iter > 50:
	# 	break
	centerShift = imageData['warpM'][-2:]
	centerShiftDist = double(sqrt(centerShift.T*centerShift))
	print 'tangent point shift of ', centerShiftDist
	linearWarp = imageData['warpM'][0:-2].reshape(2,-1)[:,-2::]
	linearWarpAmount = abs(1-linalg.det(linearWarp))
	print 'linear warp amount of ', linearWarpAmount
	
	if ((centerShiftDist > 10**-8) | (linearWarpAmount > 10**-8)) & (iter < 50):
		newCRPix = (WCS.wcstan.crpix[0] - double(centerShift[0]), WCS.wcstan.crpix[1] - double(centerShift[1]))
		WCS.wcstan.crpix[0] = newCRPix[0]
		WCS.wcstan.crpix[1] = newCRPix[1]
	
		newCRVal = WCS.pixelxy2radec(goalCRPix[0], goalCRPix[1])
		WCS.wcstan.crval[0] = newCRVal[0]
		WCS.wcstan.crval[1] = newCRVal[1]
		WCS.wcstan.crpix[0] = goalCRPix[0]
		WCS.wcstan.crpix[1] = goalCRPix[1]
		
		CD = matrix(WCS.wcstan.cd[:]).reshape(2,2)
		CD2 = (CD*linearWarp).reshape(-1,1)
		for i in arange(0,4):
			WCS.wcstan.cd[i] = CD2[i]
	
		(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
		imageData = polyWarp(imageData, catalogData, warpDegree)
	else:
		break

imageData['X'] = imageData['X_INITIAL']
imageData['Y'] = imageData['Y_INITIAL']
	
renderCatalogImage(catalogData, imageData, WCS)

if progressiveWarp:
	for deg in arange(1, warpDegree+1):
		imageData = tweakImage(imageData, catalogData, deg)
else:
	imageData = tweakImage(imageData, catalogData, warpDegree)

renderCatalogImage(catalogData, imageData, WCS)
show()	


## Write the image FITS
# for i in arange(0, imageFITS[1].data.field('X').shape[0]):
# 	imageFITS[1].data.field('X')[i] = imageData['X'][i]
# 	imageFITS[1].data.field('Y')[i] = imageData['X'][i]
# 
# if outputImageFilename != ():
# 	try:
# 		os.remove(outputImageFilename)
# 		print 'deleted existing output image file'
# 	except:
# 		junk = 1
# 	print 'writing warped image to', outputImageFilename
# 	imageFITS.writeto(outputImageFilename)
# else:
# 	print 'no output image specified, warped image not written'

# Gotta write the new WCS here, too

WCS_ground = sip.Sip('data/tweaktest4/wcs_An_Sip.fits')

