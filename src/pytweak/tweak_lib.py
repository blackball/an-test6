from pylab import *
from numpy import *
from constants import *
import pyfits
import util.sip as sip

# def loadWCS(WCSFilename):
# 	WCSFITS = pyfits.open(WCSFilename)
# 	WCSData = WCSFITS[0].header.items()
# 	return (WCSFITS, WCSData)

def WCS_ra2xy(WCS, ra, dec):
	x = zeros(ra.shape)
	y = zeros(ra.shape)
	for i in arange(0, ra.shape[0]):
		(x[i], y[i]) = WCS.radec2pixelxy(ra[i], dec[i])
	return (x,y)

def WCS_xy2rd(WCS, x, y):
	ra = zeros(x.shape)
	dec = zeros(x.shape)
	for i in arange(0, x.shape[0]):
		(ra[i], dec[i]) = WCS.pixelxy2radec(x[i], y[i])
	return (ra,dec)

def loadCatalogXYData(catalogFilename):
	catalogFITS = pyfits.open(catalogFilename)
	catalogData = {}
	catalogData['X'] = double(catalogFITS[1].data.field('X'))
	catalogData['Y'] = double(catalogFITS[1].data.field('Y'))
	return catalogData


def loadCatalogRDData(catalogFilename):
	catalogFITS = pyfits.open(catalogFilename)
	catalogData = {}
	catalogData['RA'] = double(catalogFITS[1].data.field('RA'))
	catalogData['DEC'] = double(catalogFITS[1].data.field('DEC'))
	return catalogData


def loadImageData(imageFilename):
	imageFITS = pyfits.open(imageFilename)
	imageData = {}
	imageData['X'] = double(imageFITS[1].data.field('X'))
	imageData['Y'] = double(imageFITS[1].data.field('Y'))
	imageData['FLUX'] = double(imageFITS[1].data.field('FLUX'))
	imageData['BACK'] = double(imageFITS[1].data.field('BACKGROUND'))
	imageData['X_INITIAL'] = imageData['X']
	imageData['Y_INITIAL'] = imageData['Y']
	imageData['ERRS'] = 0*imageData['X'] + 1.0
	imageData['RADII'] = 0*imageData['X'] + 1.0
	return (imageData, imageFITS)


def trimCatalog2Image(catalogData, imageData):
	inImageIdx = all([catalogData['X']>=imageData['X'].min(), catalogData['X']<=imageData['X'].max(), catalogData['Y']>=imageData['Y'].min(),catalogData['Y']<=imageData['Y'].max()],0)
	for k in catalogData.keys():
		catalogData[k] = catalogData[k][inImageIdx]
	return catalogData


def renderCatalogImage(catalogData, imageData):
	figure()
	scatter(catalogData['X'], catalogData['Y'], marker = 'o', s=30, facecolor=COLOR1, edgecolor=(1,1,1,0))
	scatter(imageData['X'], imageData['Y'], marker = 'd', s=30, facecolor=(1,1,1,0), edgecolor=COLOR2)
	axis((min(catalogData['X']), max(catalogData['X']), min(catalogData['Y']), max(catalogData['Y'])))


def findAllPairs(a, b, maxDist):
	distSqMat = tile(mat(sum(square(a),1)).T, (1,b.shape[0])) + tile(mat(sum(square(b),1)), (a.shape[0],1)) - 2*(mat(a)*mat(b).T)
	#  Identical to Matlab, but weird
	# [i,j] = where(distSqMat.T < square(maxDist))
	# pairs = concatenate((j,i),0).transpose()
	# pairs = column_stack((array(pairs)[:,0], array(pairs)[:,1]))
	# dists = array(sqrt(distSqMat[j,i]).transpose())[:,0]
	[i,j] = where(distSqMat < square(maxDist))
	pairs = concatenate((i,j),0).transpose()
	pairs = column_stack((array(pairs)[:,0], array(pairs)[:,1]))
	dists = array(sqrt(distSqMat[i,j]).transpose())[:,0]
	return (pairs, dists)


def getWeight(dists, sigmas, d_dists=(), d_sigmas=(), dd_dists=(), dd_sigmas=()):
	dos = double(dists)/double(sigmas)
	d_w = ()
	dd_w = ()
	
	if all(d_dists != (), d_sigmas != ()):
		d_dos = sigmas**(-2)*(sigmas*d_dists+(-1)*dists*d_sigmas)
		d_w = (-2)*sig**2*dos*(sig**2+dos**2)**(-2)*d_dos
	
	if all(dd_dists != (), dd_sigmas != ()):
		dd_dos = sigmas**(-3)*(2*dists*d_sigmas**2+sigmas**2*dd_dists+(-1)*sigmas*(2*d_dists*d_sigmas+dists*dd_sigmas))
		dd_w = (-2)*sig**2*(sig**2+dos**2)**(-3)*((sig**2+(-3)*dos**2)*d_dos**2+dos*(sig**2+dos**2)*dd_dos)
	
	sig = WEIGHT_SIGMA
	
	w = (sig**2)/(sig**2+dos**2)
	e = (dos**2)*w
	
	return (w, e, d_w, dd_w)


def polyExpand(imagePoints, D):
	x = imagePoints[:,0]
	y = imagePoints[:,1]
	
	width = ((D+1)*(D+2))/2-1
	A_sub = double(mat(zeros((x.shape[0], width))))
	col = 0
	for d in arange(D, 0, -1):
		for i in arange(0, d+1):
			A_sub[:,col] = multiply(power(x,(d-i)), power(y,i))
			col = col + 1;
	
	A = zeros((2*x.shape[0], 2*(width)+2))
	A[::2,0:width] = A_sub
	A[1::2,width:(2*width)] = A_sub
	A[:,(2*width):(2*width+2)] = tile(mat([[1.,0.],[0.,1.]]), (x.shape[0],1))
	return A


def polyWarp(imagePoints_all, imagePoints, catalogPoints, warpWeights, warpDegree):
	
	b = catalogPoints.reshape(-1,1)
	A = polyExpand(imagePoints, warpDegree)
	A_all = polyExpand(imagePoints_all, warpDegree)
	
	if warpWeights.shape[1] < 2:
		warpWeights_double = tile(warpWeights, (1,2)).reshape(-1,1)
	else:
		warpWeights_double = warpWeights.reshape(-1,1)
	
	WA = multiply(warpWeights_double.repeat(A.shape[1],1), A)
	Wb = multiply(warpWeights_double, b)
	M = linalg.lstsq(WA,Wb)[0]
	
	residuals = (Wb - WA*M)
	
	# Not sure if this is correct for warpDegree > 1
	# W = M[0:M.shape[0]-2].reshape(-1,2)
	# t = M[M.shape[0]-2:M.shape[0]]
	
	imagePoints_warp = (A_all*M).reshape(-1,2)
	
	return (imagePoints_warp, residuals)


def tweakImage(imageData, catalogData, warpDegree):
	MINWEIGHT = (getWeight(MINWEIGHT_DOS,1))[0]
	MAXERROR = MINWEIGHT*(MINWEIGHT_DOS**2);

	minSize = array([catalogData['SIGMA_X'].shape[0], imageData['ERRS'].shape[0]]).min()

	maxSigmas = mean(sqrt(column_stack((imageData['ERRS'][1:minSize], imageData['ERRS'][1:minSize]))**2 + column_stack((catalogData['SIGMA_X'][1:minSize], catalogData['SIGMA_Y'][1:minSize]))**2),1)
	maxSigma = mean(maxSigmas) + 3*std(maxSigmas)

	maxImageDist = MAXDIST_MULT*sqrt(maxSigma*((WEIGHT_SIGMA**2)*(1-MINWEIGHT)/MINWEIGHT))

	numAllPairs = imageData['X'].shape[0]*catalogData['X'].shape[0]

	totalResiduals = []
	redist_countdown = 0

	print 'tweaking image with degree =', warpDegree

	for iter in arange(0,TWEAK_MAX_NUM_ITERS):

		just_requeried = 0;
		if redist_countdown == 0:
			(pairs_all, dists_all) = findAllPairs(column_stack((imageData['X'], imageData['Y'])), column_stack((catalogData['X'], catalogData['Y'])), maxImageDist)
			sigmas_all = (sqrt(imageData['ERRS'][pairs_all[:,0]]**2 + catalogData['SIGMA_X'][pairs_all[:,1]]**2) + sqrt(imageData['ERRS'][pairs_all[:,0]]**2 + catalogData['SIGMA_Y'][pairs_all[:,1]]**2))/2
			just_requeried = 1
			redist_countdown = REDIST_INTERVAL
		else:
			dists_all = sqrt(square(imageData['X'][pairs_all[:,0]] - catalogData['X'][pairs_all[:,1]]) + square(imageData['Y'][pairs_all[:,0]] - catalogData['Y'][pairs_all[:,1]]))

		weights_all = getWeight(dists_all, sigmas_all)[0]

		toKeep = weights_all > MINWEIGHT
		numDropped = numAllPairs - sum(toKeep)
		pairs = pairs_all[toKeep,:]
		sigmas = sigmas_all[toKeep,:]
		dists = dists_all[toKeep,:]
		weights = weights_all[toKeep,:]

		imagePoints_all = matrix(column_stack((imageData['X_INITIAL'], imageData['Y_INITIAL'])))
		imagePoints = matrix(column_stack((imageData['X_INITIAL'][pairs[:,0]], imageData['Y_INITIAL'][pairs[:,0]])))
		catalogPoints = matrix(column_stack((catalogData['X'][pairs[:,1]], catalogData['Y'][pairs[:,1]])))
		warpWeights = matrix(sqrt(weights)/sigmas).T

		tmp = polyWarp(imagePoints_all, imagePoints, catalogPoints, warpWeights, warpDegree)
		newPoints = tmp[0]
		residuals = tmp[1]

		imageData['X'] = array(newPoints[:,0])[:,0]
		imageData['Y'] = array(newPoints[:,1])[:,0]

		totalResiduals.append(numDropped*MINWEIGHT*(MINWEIGHT_DOS**2) + sum(square(residuals)))

		redist_countdown = redist_countdown - 1
		if (iter > 3) & all(abs(totalResiduals[-3:-1] - totalResiduals[-1]) <= MIN_IRLS_ACCURACY):
			if just_requeried:
				break
			else:
				redist_countdown = 0
	
	print 'image tweaked in', iter, 'iterations'
	
	return imageData

