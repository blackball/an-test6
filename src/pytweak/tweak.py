from numpy import *
from pylab import *
from tweak_lib import *
from constants import *
import getopt
import util.sip as sip


def tweak(inputWCSFilename, catalogRDFilename, imageXYFilename,
          outputWCSFilename, catalogXYFilename, imageRDFilename, warpDegree,
		  progressiveWarp=DEFAULT_PROGRESSIVE_WARP,
		  renderOutput=DEFAULT_RENDER_OUTPUT, goal_CRPix_X=(),
		  goal_CRPix_Y=()):
	
	if warpDegree < 1:
		print 'warning: order set to less than 1, resetting order to 1'
		warpDegree = 1
	
	(imageData, catalogData, WCS) = loadData(imageXYFilename, catalogRDFilename, inputWCSFilename, warpDegree)
	
	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Initial WCS')
		savefig('1_Initial.png')

	goal_crpix = [WCS.wcstan.imagew/2 + 0.5, WCS.wcstan.imageh/2 + 0.5]
	#  We add the 0.5 to account for the pixel representation, where center = 1. Right?
	
	if goal_CRPix_X != ():
		goal_crpix[0] = goal_CRPix_X
	if goal_CRPix_Y != ():
		goal_crpix[1] = goal_CRPix_Y
	
	fixCRPix(imageData, catalogData, WCS, goal_crpix, progressiveWarp, renderOutput)
		
	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Fixed CRPix')
		savefig('3_Fixed_CRPix.png')
	
	if progressiveWarp:
		tweakImage_progressive(imageData, catalogData, WCS)
	else:
		tweakImage(imageData, catalogData, WCS)
	
	polyWarpWCS_repeat(imageData, catalogData, WCS)	
	
	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Fit (TAN)')
		savefig('4_TAN.png')
	
	pushPoly2WCS(WCS)
	
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
	
	goal_CRPix_X = ()
	goal_CRPix_Y = ()

	# Debug stuff
	folder = 'data/tweaktest4/'
	catalogRDFilename = folder + 'index.rd.fits'
	imageXYFilename = folder + 'field.xy.fits'
	inputWCSFilename = folder + 'wcs.fits'
	
	catalogXYFilename = folder + 'index.xy.fits'
	imageRDFilename = folder + 'field.rd.fits'
	outputWCSFilename = folder + 'out.wcs.fits'
	renderOutput = True
	
	(opts, args) = getopt.getopt(sys.argv[1:], '', ['catalog_rd=', 'catalog_xy=', 'image_rd=', 'image_xy=', 'wcs_in=', 'wcs_out=', 'order=', 'crpix_x=', 'crpix_y=', 'progressive', 'display'])
	
	for opt in opts:
		
		if opt[0] == '--catalog_rd':
			catalogRDFilename = opt[1]
		elif opt[0] == '--image_xy':
			imageXYFilename = opt[1]
		elif opt[0] == '--wcs_in':
			inputWCSFilename = opt[1]
		elif opt[0] == '--catalog_xy':
			catalogXYFilename = opt[1]
		elif opt[0] == '--image_rd':
			imageRDFilename = opt[1]
		elif opt[0] == '--wcs_out':
			outputWCSFilename = opt[1]
		elif opt[0] == '--order':
			warpDegree = int(opt[1])
		elif opt[0] == '--crpix_x':
			goal_CRPix_X = float(opt[1])
		elif opt[0] == '--crpix_y':
			goal_CRPix_Y = float(opt[1])
		elif opt[0] == '--progressive':
			progressiveWarp = True
		elif opt[0] == '--display':
			renderOutput = True

	if ((imageXYFilename == ()) | (catalogRDFilename == ()) | (inputWCSFilename == ())):
		print 'insufficient input arguments'
		print 'usage: python tweak.py --wcs_in=INPUT_WCS_FITS --catalog_rd=CATALOG_RD_FITS --image_xy=IMAGE_XY_FITS [--catalog_xy CATALOG_XY_FITS --image_rd IMAGE_RD_FITS --wcs_out OUTPUT_WCS_FITS --order WARP_ORDER --progressive --display]'
		print ' [--progressive] Does progressive warping'
		print ' [--render] Renders visible output'
		raise SystemExit

	if warpDegree == ():
		print 'warning: warp degree not specified, using default of', DEFAULT_WARP_DEGREE
		warpDegree = DEFAULT_WARP_DEGREE
	
	if (goal_CRPix_X == ()) & (goal_CRPix_Y == ()):
		print 'warning: no crpix values specified, defaulting to image center'
	elif not((goal_CRPix_X != ()) & (goal_CRPix_Y != ())):
		print 'warning: only one crpix value specified. Unspecified value defaulting to image center'
		
	tweak(inputWCSFilename, catalogRDFilename, imageXYFilename, outputWCSFilename, catalogXYFilename, imageRDFilename, warpDegree, progressiveWarp, renderOutput, goal_CRPix_X, goal_CRPix_Y)

