from numpy import *
from pylab import *
from tweak_lib import *
from constants import *
import getopt
import os
import util.sip as sip


def tweak(inputWCSFilename, catalogRDFilename, imageXYFilename, outputWCSFilename, catalogXYFilename, imageRDFilename, warpDegree, progressiveWarp=False, renderOutput=False):
	imageData = loadImageData(imageXYFilename)[0]
	catalogData = loadFITS(catalogRDFilename, ['RA', 'DEC'])[0]
	WCS = sip.Sip(inputWCSFilename)

	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])

	# Fudge factor until we get the real data
	catalogData['SIGMA_X'] = 0*catalogData['X'] + 1.0
	catalogData['SIGMA_Y'] = 0*catalogData['X'] + 1.0

	if renderOutput:
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

	if renderOutput:
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
		print '\nwriting new WCS to disk'
		try:
			os.remove(outputWCSFilename)
			print 'deleted existing ' + outputWCSFilename
		except:
			junk = 1

		WCS.write_to_file(outputWCSFilename)
		print 'WCS written to ' + outputWCSFilename
		print 'rereading WCS header'
	
		WCSFITS_old = pyfits.open(inputWCSFilename)
		WCSFITS_new = pyfits.open(outputWCSFilename)

		for history in WCSFITS_old[0].header.get_history():
			WCSFITS_new[0].header.add_history(history)

		for comment in WCSFITS_old[0].header.get_comment():
			if comment[0:5] == 'Tweak':
				WCSFITS_new[0].header.add_comment('Tweak: yes')
				WCSFITS_new[0].header.add_comment('Tweak AB order: ' + str(WCS.a_order))
				WCSFITS_new[0].header.add_comment('Tweak ABP order: ' + str(WCS.ap_order))
			else:
				WCSFITS_new[0].header.add_comment(comment)

		WCSFITS_new[0].header.add_comment('AN_JOBID: ' + WCSFITS_old[0].header.get('AN_JOBID'))
		WCSFITS_new[0].header.add_comment('DATE: ' + WCSFITS_old[0].header.get('DATE'))
		WCSFITS_new[0].update_header()
	
		try:
			os.remove(outputWCSFilename)
			print 'deleted existing ' + outputWCSFilename + ' again'
		except:
			junk = 1
	
		pyfits.writeto(outputWCSFilename, array([]), WCSFITS_new[0].header)
	
		print 'WCS + Comments/History written to ' + outputWCSFilename
	
		WCS_out = sip.Sip(outputWCSFilename)
	
		if catalogXYFilename != ():
			print '\ncalling wcs-rd2xy'
			if os.system('./wcs-rd2xy -w ' + outputWCSFilename + ' -i ' + catalogRDFilename + ' -o ' + catalogXYFilename):
				print 'failed to convert catalog RA/Dec -> XY'
			else:
				print 'catalog X/Y written to ' + catalogXYFilename
	
		if imageRDFilename != ():
			print '\ncalling wcs-xy2rd'
			if os.system('./wcs-xy2rd -w ' + outputWCSFilename + ' -i ' + imageXYFilename + ' -o ' + imageRDFilename):
				print 'failed to convert image X/Y -> RA/Dec'
			else:
				print 'image RA/Dec written to ' + imageRDFilename
	
		if renderOutput:
	
			if catalogXYFilename != ():
				catalogXYData = loadFITS(catalogXYFilename, ['X', 'Y'])[0]
				renderCatalogImage(catalogXYData, imageData, WCS_out)
				title('Fit (With SIP)')
				savefig('3-after-NoSIP.png')
			else:
				print 'not rendering warped catalog because catalog output not specified'
	
			if imageRDFilename != ():
				imageRDData = loadFITS(imageRDFilename, ['RA', 'DEC'])[0]
				renderCatalogImageRADec(catalogData, imageRDData, WCS_out)
				title('Fit (With SIP) on Sphere')
				savefig('4-after-NoSIP-sphere.png')
			else:
				print 'not rendering warped image because image output not specified'
	else:
		print 'not writing WCS output, so not writing any output!'

	if renderOutput:	
		show()




if __name__ == '__main__':
	
	catalogRDFilename = ()
	imageXYFilename = ()
	inputWCSFilename = ()

	catalogXYFilename = ()
	imageRDFilename = ()
	outputWCSFilename = ()

	warpDegree = ()
	progressiveWarp = DEFAULT_PROGRESSIVE_WARP
	renderOutput = False

	# # Debug stuff
	# folder = 'data/tweaktest1/'
	# catalogRDFilename = folder + 'index.rd.fits'
	# imageXYFilename = folder + 'field.xy.fits'
	# inputWCSFilename = folder + 'wcs.fits'
	# 
	# catalogXYFilename = folder + 'index.xy.fits'
	# imageRDFilename = folder + 'field.rd.fits'
	# outputWCSFilename = folder + 'out.wcs.fits'
	# renderOutput = True
	
	(opts, args) = getopt.getopt(sys.argv[1:], 'i:f:w:x:r:s:d:pv')

	for opt in opts:

		if opt[0] == '-i':
			catalogRDFilename = opt[1]
		elif opt[0] == '-f':
			imageXYFilename = opt[1]
		elif opt[0] == '-w':
			inputWCSFilename = opt[1]
		elif opt[0] == '-x':
			catalogXYFilename = opt[1]
		elif opt[0] == '-r':
			imageRDFilename = opt[1]
		elif opt[0] == '-s':
			outputWCSFilename = opt[1]
		elif opt[0] == '-d':
			warpDegree = int(opt[1])
		elif opt[0] == '-p':
			progressiveWarp = True
		elif opt[0] == '-v':
			renderOutput = True

	if ((imageXYFilename == ()) | (catalogRDFilename == ()) | (inputWCSFilename == ())):
		print 'insufficient input arguments'
		print 'usage: python tweak.py -w WCS_FITS -i index_RD_FITS -f field_XY_FITS [-x index_XY_FITS -r field_RD_FITS -s output_WCS_FITS -d order -p -r]'
		print '-p does progressive warping'
		print '-v renders visible output'
		raise SystemExit

	if warpDegree == ():
		print 'warp degree not specified, using default of', DEFAULT_WARP_DEGREE
		warpDegree = DEFAULT_WARP_DEGREE

	tweak(inputWCSFilename, catalogRDFilename, imageXYFilename, outputWCSFilename, catalogXYFilename, imageRDFilename, warpDegree, progressiveWarp, renderOutput)

