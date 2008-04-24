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
outputWCSFilename = ()
warpDegree = ()
progressiveWarp = DEFAULT_PROGRESSIVE_WARP

# Debug stuff
folder = 'data/tweaktest1/'
catalogRDFilename = folder + 'index.rd.fits'
WCSFilename = folder + 'wcs.fits'
imageFilename = folder + 'field.xy.fits'
outputWCSFilename = folder + 'out.wcs.fits'

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

(catalogData, catalogFITS) = loadCatalogRDData(catalogRDFilename)

(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])

# Fudge factor until we get the real data
catalogData['SIGMA_X'] = 0*catalogData['X'] + 1.0
catalogData['SIGMA_Y'] = 0*catalogData['X'] + 1.0

renderCatalogImage(catalogData, imageData, WCS)
title('Original Fit')
savefig('1-initial.png')

tweakImage(imageData, catalogData, warpDegree)

imageData['PIVOT'][0] = WCS.wcstan.imagew/2 + 0.5
imageData['PIVOT'][1] = WCS.wcstan.imageh/2 + 0.5
#  We add the 0.5 to account for the pixel representation, where center = 1. Right?

startCRPix = array([WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]])
(WCS.wcstan.crval[0], WCS.wcstan.crval[1]) = WCS.pixelxy2radec(imageData['PIVOT'][0], imageData['PIVOT'][1])
(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = (imageData['PIVOT'][0], imageData['PIVOT'][1])

(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
polyWarp(imageData, catalogData, warpDegree)


iter = 0
MAX_TAN_ITERS = 20
MAX_SHIFT_AMOUNT = 10**-8
MAX_LWARP_AMOUNT = 10**-8
while True:
	iter = iter + 1
	
	if progressiveWarp:
		for deg in arange(1, warpDegree+1):
			tweakImage(imageData, catalogData, deg)
	else:
		tweakImage(imageData, catalogData, warpDegree)
	
	centerShift = array(imageData['warpM'].reshape(2,-1)[:,-1].T)[0]
	centerShiftDist = sqrt(sum(square(centerShift)))
	linearWarp = imageData['warpM'].reshape(2,-1)[:,-3:-1]
	linearWarpAmount = sqrt(sum(square(array(linearWarp - eye(2,2)))))
	# linearWarpAmount = abs(1-linalg.det(linearWarp))
	
	print '(shift, warp) = ', (centerShiftDist, linearWarpAmount)
	
	if ((centerShiftDist > MAX_SHIFT_AMOUNT) | (linearWarpAmount > MAX_LWARP_AMOUNT)) & (iter < MAX_TAN_ITERS):
		
		startCRPix = array([WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]])
		
		(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = startCRPix - centerShift
		(WCS.wcstan.crval[0], WCS.wcstan.crval[1]) = WCS.pixelxy2radec(startCRPix[0], startCRPix[1])
		(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = startCRPix
		
		CD = matrix(WCS.wcstan.cd[:]).reshape(2,2)
		CD2 = (CD*linearWarp).reshape(-1,1)
		for i in arange(0,4):
			WCS.wcstan.cd[i] = CD2[i]
		
		(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
		
		# This...
		Mpoly = imageData['warpM'].reshape(2,-1)[:,:-3].reshape(-1,1).copy()
		xy = matrix(column_stack((imageData['X_INITIAL'] - imageData['PIVOT'][0], imageData['Y_INITIAL']  - imageData['PIVOT'][1])))
		xy_warp = xy + (polyExpand(xy, warpDegree,2)*Mpoly).reshape(-1,2)
		imageData['X'] = array(xy_warp[:,0])[:,0] + imageData['PIVOT'][0]
		imageData['Y'] = array(xy_warp[:,1])[:,0] + imageData['PIVOT'][1]
		
		# is effectively equivalent to this, but much faster and more elegant:
		# polyWarp(imageData, catalogData, warpDegree)
	else:
		break


imageData['X'] = imageData['X_INITIAL']
imageData['Y'] = imageData['Y_INITIAL']

renderCatalogImage(catalogData, imageData, WCS)
title('Fit (No SIP)')
savefig('2-after-SIP.png')

minDeg_im2cat = 2
maxDeg_im2cat = warpDegree
SIP_im2cat = imageData['warpM'].reshape(2,-1)[:,:-3].reshape(-1,1).copy()

interval = 0.025
count = int(1/interval)+1
gridBase = 1.0 - 2.0*matrix(arange(0, 1+interval, interval)).T

gridStart = concatenate((repeat( (WCS.wcstan.imagew-imageData['PIVOT'][0]) * gridBase, count, 0), tile( (WCS.wcstan.imageh-imageData['PIVOT'][1]) * gridBase, (count,1))), 1)
gridEnd = gridStart + (polyExpand(gridStart, warpDegree,2)*SIP_im2cat).reshape(-1,2)

minDeg_cat2im = 1
maxDeg_cat2im = warpDegree
SIP_cat2im = linalg.lstsq(polyExpand(gridEnd, maxDeg_cat2im, minDeg_cat2im),(gridStart-gridEnd).reshape(-1,1))[0]
gridStart_approx = gridEnd + (polyExpand(gridEnd, maxDeg_cat2im, minDeg_cat2im)*SIP_cat2im).reshape(-1,2)

print 'warp inversion error:', sum(abs(gridStart_approx - gridStart),0)/gridStart.shape[0]

# figure()
# scatter(array(gridEnd[:,0]).T[0], array(gridEnd[:,1]).T[0], marker = 'o', s=30, facecolor=COLOR1, edgecolor=(1,1,1,0))
# scatter(array(gridEnd_approx[:,0]).T[0], array(gridEnd_approx[:,1]).T[0], marker = 'd', s=30, facecolor=(1,1,1,0), edgecolor=COLOR2)
# axis('image')

WCS.a_order = maxDeg_im2cat
WCS.b_order = maxDeg_im2cat

col = 0
for d in arange(maxDeg_im2cat, minDeg_im2cat-1, -1):
	for i in arange(0, d+1):
		idx = (d-i)*10+i
		WCS.a[idx] = SIP_im2cat[col]
		WCS.b[idx] = SIP_im2cat[col+SIP_im2cat.shape[0]/2]
		col = col + 1;


WCS.ap_order = maxDeg_cat2im
WCS.bp_order = maxDeg_cat2im

col = 0
for d in arange(maxDeg_cat2im, minDeg_cat2im-1, -1):
	for i in arange(0, d+1):
		idx = (d-i)*10+i
		WCS.ap[idx] = SIP_cat2im[col]
		WCS.bp[idx] = SIP_cat2im[col+SIP_cat2im.shape[0]/2]
		col = col + 1;

if outputWCSFilename != ():
	try:
		os.remove(outputWCSFilename)
		print 'deleted existing output WCS file'
	except:
		junk = 1

	WCS.write_to_file(outputWCSFilename)
	print 'WCS writtent to disk'
	WCS_out = sip.Sip(outputWCSFilename)

(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS_out, catalogData['RA'], catalogData['DEC'])
renderCatalogImage(catalogData, imageData, WCS_out)
title('Fit (With SIP)')
savefig('3-after-NoSIP.png')

(imageData['RA'], imageData['DEC']) = WCS_xy2rd(WCS_out, imageData['X'], imageData['Y'])
renderCatalogImageRADec(catalogData, imageData, WCS_out)
title('Fit (With SIP) on Sphere')
savefig('4-after-NoSIP-sphere.png')
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
