from numpy import *
from pylab import *
from tweak_lib import *
from constants import *
import getopt
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
	
	for iter in range(1, MAX_CAMERA_ITERS):
		if progressiveWarp:
			for deg in arange(1, warpDegree+1):
				WCS.warpDegree = deg
				tweakImage(imageData, catalogData, WCS)
			WCS.warpDegree = warpDegree
		else:
			tweakImage(imageData, catalogData, WCS)
		
		imageData['X'] = imageData['X_INITIAL']
		imageData['Y'] = imageData['Y_INITIAL']
		
		centerShiftDist = sqrt(sum(square(array(imageData['warpM'].reshape(2,-1)[:,-1].T)[0])))
		linearWarpAmount = sqrt(sum(square(array(imageData['warpM'].reshape(2,-1)[:,-3:-1] - eye(2,2)))))
		# linearWarpAmount = abs(1-linalg.det(linearWarp))
				
		print '(shift, warp) = ', (centerShiftDist, linearWarpAmount)

		affinewarp2WCS(imageData, WCS)
		(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])	
	
		if ((centerShiftDist < MAX_SHIFT_AMOUNT) & (linearWarpAmount < MAX_LWARP_AMOUNT)):
			break
	
	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Fit (No SIP)')
		savefig('2-after-SIP.png')
	
	polywarp2WCS(imageData, WCS)
	
	writeOutput(WCS, inputWCSFilename, outputWCSFilename, catalogXYFilename, catalogRDFilename, imageXYFilename, imageRDFilename, renderOutput)
	
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
	renderOutput = DEFAULT_RENDER_OUTPUT

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

