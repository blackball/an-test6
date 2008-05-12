from numpy import *
from pylab import *
from tweak_lib import *
from constants import *
import getopt
import os
import util.sip as sip


def tweak(inputWCSFilename, catalogRDFilename, imageXYFilename,
          outputWCSFilename, catalogXYFilename, imageRDFilename, warpDegree,
		  progressiveWarp=DEFAULT_PROGRESSIVE_WARP,
		  renderOutput=DEFAULT_RENDER_OUTPUT):
	
	(imageData, catalogData, WCS) = loadData(imageXYFilename, catalogRDFilename, inputWCSFilename, warpDegree)
	
	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Original Fit')
		savefig('1-initial.png')

	tweakImage(imageData, catalogData, WCS)

	imageData['PIVOT'][0] = WCS.wcstan.imagew/2 + 0.5
	imageData['PIVOT'][1] = WCS.wcstan.imageh/2 + 0.5
	#  We add the 0.5 to account for the pixel representation, where center = 1. Right?
	
	fixCRPix(imageData, catalogData, WCS)
		
	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Fixed CRPix')
		savefig('1-crpix.png')
	
	iter = 0
	while True:
		iter = iter + 1
	
		if progressiveWarp:
			for deg in arange(1, warpDegree+1):
				WCS.warpDegree = deg
				tweakImage(imageData, catalogData, WCS)
			WCS.warpDegree = warpDegree
		else:
			tweakImage(imageData, catalogData, WCS)
	
		centerShiftDist = sqrt(sum(square(array(imageData['warpM'].reshape(2,-1)[:,-1].T)[0])))
		linearWarpAmount = sqrt(sum(square(array(imageData['warpM'].reshape(2,-1)[:,-3:-1] - eye(2,2)))))
		# linearWarpAmount = abs(1-linalg.det(linearWarp))
	
		print '(shift, warp) = ', (centerShiftDist, linearWarpAmount)
	
		if ((centerShiftDist > MAX_SHIFT_AMOUNT) | (linearWarpAmount > MAX_LWARP_AMOUNT)) & (iter < MAX_TAN_ITERS):
			
			affinewarp2WCS(imageData, WCS)
					
			(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
			imageData['X'] = imageData['X_INITIAL']
			imageData['Y'] = imageData['Y_INITIAL']
			
		else:
			break
	
	imageData['X'] = imageData['X_INITIAL']
	imageData['Y'] = imageData['Y_INITIAL']


	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Fit (No SIP)')
		savefig('2-after-SIP.png')
	
	polywarp2WCS(imageData, WCS)

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

	# Debug stuff
	folder = 'data/tweaktest2/'
	catalogRDFilename = folder + 'index.rd.fits'
	imageXYFilename = folder + 'field.xy.fits'
	inputWCSFilename = folder + 'wcs.fits'
	
	catalogXYFilename = folder + 'index.xy.fits'
	imageRDFilename = folder + 'field.rd.fits'
	outputWCSFilename = folder + 'out.wcs.fits'
	renderOutput = True
	
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

