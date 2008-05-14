from pylab import *
from numpy import *
from constants import *
import pyfits
import util.sip as sip
import os

import pdb

def loadData(imageXYFilename, catalogRDFilename, inputWCSFilename, warpDegree):
	imageData = loadImageData(imageXYFilename)[0]
	catalogData = loadFITS(catalogRDFilename, ['RA', 'DEC'])[0]
	WCS = sip.Sip(inputWCSFilename)
	
	WCS.warpDegree = warpDegree;
	
	WCS.a_order = WCS.warpDegree
	WCS.b_order = WCS.warpDegree
	WCS.ap_order = WCS.warpDegree
	WCS.bp_order = WCS.warpDegree
	
	WCS.ab_order_min = MIN_AB_ORDER
	WCS.abp_order_min = MIN_ABP_ORDER

	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])

	# Fudge factor until we get the real data
	catalogData['SIGMA_X'] = 0*catalogData['X'] + 1.0
	catalogData['SIGMA_Y'] = 0*catalogData['X'] + 1.0
	
	return (imageData, catalogData, WCS)

def fixCRPix(imageData, catalogData, WCS, goal_crpix, renderOutput=False):
	
	tweakImage(imageData, catalogData, WCS)	
	if renderOutput:
		renderCatalogImage(catalogData, imageData, WCS)
		title('Tweaked (Fiducial CRPix)')
		savefig('2_Tweaked_Fiducial.png')
	
	imageData['X'] = imageData['X_INITIAL']
	imageData['Y'] = imageData['Y_INITIAL']
	
	startCRPix = array([WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]])
	(WCS.wcstan.crval[0], WCS.wcstan.crval[1]) = WCS.pixelxy2radec(goal_crpix[0], goal_crpix[1])
	(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = (goal_crpix[0], goal_crpix[1])
	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
	
	polyWarpWCS_repeat(imageData, catalogData, WCS)

def WCS_rd2xy(WCS, ra, dec):
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

def loadFITS(filename, fields):
	FITS = pyfits.open(filename)
	data = {}
	for f in fields:
		data[f] = double(FITS[1].data.field(f))
	return (data, FITS)

def loadImageData(imageFilename):
	(imageData, imageFITS) = loadFITS(imageFilename, ['X', 'Y', 'FLUX', 'BACKGROUND'])
	imageData['X_INITIAL'] = imageData['X']
	imageData['Y_INITIAL'] = imageData['Y']
	
	# These two are also fudged:
	imageData['ERRS'] = 0*imageData['X'] + 1.0
	imageData['RADII'] = 0*imageData['X'] + 1.0
	
	return (imageData, imageFITS)

# Using this is a bad idea, probably
# def trimCatalog2Image(catalogData, imageData):
# 	inImageIdx = all([catalogData['X']>=imageData['X'].min(), catalogData['X']<=imageData['X'].max(), catalogData['Y']>=imageData['Y'].min(),catalogData['Y']<=imageData['Y'].max()],0)
# 	for k in catalogData.keys():
# 		catalogData[k] = catalogData[k][inImageIdx]
# 	return catalogData


def renderCatalogImage(catalogData, imageData, WCS):
	figure()
	scatter(catalogData['X'], catalogData['Y'], marker = 'o', s=30, facecolor=COLOR1, edgecolor=(1,1,1,0))
	scatter(imageData['X'], imageData['Y'], marker = 'd', s=30, facecolor=(1,1,1,0), edgecolor=COLOR2)
	scatter(array([WCS.wcstan.crpix[0]]), array([WCS.wcstan.crpix[1]]), marker = '^', s=200, facecolor=(0.3,1,0.5,0.3), edgecolor=(0,0,0,0.5))
	axis('image')
	try:
		axis((min(imageData['X_INITIAL']), max(imageData['X_INITIAL']), min(imageData['Y_INITIAL']), max(imageData['Y_INITIAL'])))
	except:
		axis((min(imageData['X']), max(imageData['X']), min(imageData['Y']), max(imageData['Y'])))

def renderCatalogImageRADec(catalogData, imageData, WCS):
	figure()
	scatter(catalogData['RA'], catalogData['DEC'], marker = 'o', s=30, facecolor=COLOR1, edgecolor=(1,1,1,0))
	scatter(imageData['RA'], imageData['DEC'], marker = 'd', s=30, facecolor=(1,1,1,0), edgecolor=COLOR2)
	scatter(array([WCS.wcstan.crval[0]]), array([WCS.wcstan.crval[1]]), marker = '^', s=200, facecolor=(0.3,1,0.5,0.3), edgecolor=(0,0,0,0.5))
	axis('image')


def renderImageMotion(imageData, WCS):
	figure()
	scatter(imageData['X_INITIAL'], imageData['Y_INITIAL'], marker = 'o', s=30, facecolor=(1,1,1,0), edgecolor=COLOR1)
	scatter(imageData['X'], imageData['Y'], marker = 'd', s=30, facecolor=(1,1,1,0), edgecolor=COLOR2)
	scatter(array([WCS.wcstan.crpix[0]]), array([WCS.wcstan.crpix[1]]), marker = '^', s=200, facecolor=(0.3,1,0.5,0.3), edgecolor=(0,0,0,0.5))
	axis('image')
	axis((min(imageData['X_INITIAL']), max(imageData['X_INITIAL']), min(imageData['Y_INITIAL']), max(imageData['Y_INITIAL'])))


def findAllPairs(a, b, maxDist):
	distSqMat = tile(mat(sum(square(a),1)).T, (1,b.shape[0])) + tile(mat(sum(square(b),1)), (a.shape[0],1)) - 2*(mat(a)*mat(b).T)
	#  Identical to Matlab
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


def polyExpand(imagePoints, D, Dstart = 0):
	x = imagePoints[:,0]
	y = imagePoints[:,1]
	
	width = (((D+1)*(D+2))/2-1) - ((Dstart*(Dstart+1))/2-1)
	A_sub = double(mat(zeros((x.shape[0], width))))
	
	col = 0
	for d in arange(D, Dstart-1, -1):
		for i in arange(0, d+1):
			A_sub[:,col] = multiply(power(x,(d-i)), power(y,i))
			col = col + 1;
	
	A = zeros((2*x.shape[0], 2*(width)))
	A[::2,0:width] = A_sub
	A[1::2,width:(2*width)] = A_sub
	return A

def polyWarpWCS_repeat(imageData, catalogData, WCS):
	for iter in range(1, MAX_CAMERA_ITERS):
		(centerShiftDist, linearWarpAmount) = polyWarpWCS(imageData, catalogData, WCS)	
		if ((centerShiftDist < MAX_SHIFT_AMOUNT) & (linearWarpAmount < MAX_LWARP_AMOUNT)):
				break
	

def polyWarpWCS(imageData, catalogData, WCS):
	polyWarp(imageData, catalogData, WCS)
	
	centerShiftDist = sqrt(sum(square(array(WCS.warpM.reshape(2,-1)[:,-1].T)[0])))
	linearWarpAmount = sqrt(sum(square(array(WCS.warpM.reshape(2,-1)[:,-3:-1] - eye(2,2)))))
	# linearWarpAmount = abs(1-linalg.det(linearWarp))
			
	print '(shift, warp) = ', (centerShiftDist, linearWarpAmount)
	
	pushAffine2WCS(WCS)
	
	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
	imageData['X'] = imageData['X_INITIAL']
	imageData['Y'] = imageData['Y_INITIAL']
	
	return (centerShiftDist, linearWarpAmount)

def polyWarp(imageData, catalogData, WCS):
	pivot = WCS.wcstan.crpix
	pairs = imageData['pairs']
	x_init = imageData['X_INITIAL'] - pivot[0]
	y_init = imageData['Y_INITIAL'] - pivot[1]
	x = imageData['X'] - pivot[0]
	y = imageData['Y'] - pivot[1]
	cx = catalogData['X'] - pivot[0]
	cy = catalogData['Y'] - pivot[1]
	
	imagePoints_all = matrix(column_stack((x_init, y_init)))
	imagePoints = matrix(column_stack((x_init[pairs[:,0]], y_init[pairs[:,0]])))
	catalogPoints = matrix(column_stack((cx[pairs[:,1]], cy[pairs[:,1]])))
	warpWeights = matrix(sqrt(imageData['weights'])/imageData['sigmas']).T
	
	if warpWeights.shape[1] < 2:
		warpWeights_double = tile(warpWeights, (1,2)).reshape(-1,1)
	else:
		warpWeights_double = warpWeights.reshape(-1,1)
	
	b = catalogPoints.reshape(-1,1)
	A = polyExpand(imagePoints, WCS.warpDegree)
	A_all = polyExpand(imagePoints_all, WCS.warpDegree)
	
	WA = multiply(warpWeights_double.repeat(A.shape[1],1), A)
	Wb = multiply(warpWeights_double, b)
	M = linalg.lstsq(WA,Wb)[0]
	resid = (Wb - WA*M)
	
	# A = polyExpand(catalogPoints, warpDegree)
	# WA = multiply(warpWeights_double.repeat(A.shape[1],1), A)
	# Wb = multiply(warpWeights_double, imagePoints.reshape(-1,1))
	# iM = linalg.lstsq(WA,Wb)[0]
		
	imagePoints_warp = (A_all * M).reshape(-1,2)
	
	imageData['X'] = array(imagePoints_warp[:,0])[:,0] + pivot[0]
	imageData['Y'] = array(imagePoints_warp[:,1])[:,0] + pivot[1]
	imageData['residuals'] = resid
	WCS.warpM = M
	# imageData['iwarpM'] = iM


def tweakImage(imageData, catalogData, WCS):
	MINWEIGHT = (getWeight(MINWEIGHT_DOS,1))[0]
	MAXERROR = MINWEIGHT*(MINWEIGHT_DOS**2);

	minSize = array([catalogData['SIGMA_X'].shape[0], imageData['ERRS'].shape[0]]).min()

	maxSigmas = mean(sqrt(column_stack((imageData['ERRS'][1:minSize], imageData['ERRS'][1:minSize]))**2 + column_stack((catalogData['SIGMA_X'][1:minSize], catalogData['SIGMA_Y'][1:minSize]))**2),1)
	maxSigma = mean(maxSigmas) + 3*std(maxSigmas)

	maxImageDist = MAXDIST_MULT*sqrt(maxSigma*((WEIGHT_SIGMA**2)*(1-MINWEIGHT)/MINWEIGHT))

	numAllPairs = imageData['X'].shape[0]*catalogData['X'].shape[0]

	totalResiduals = []
	redist_countdown = 0

	# print 'tweaking image with degree =', WCS.warpDegree

	for iter in range(0,TWEAK_MAX_NUM_ITERS):
				
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
		imageData['pairs'] = pairs_all[toKeep,:]
		imageData['sigmas'] = sigmas_all[toKeep,:]
		imageData['dists'] = dists_all[toKeep,:]
		imageData['weights'] = weights_all[toKeep,:]
		
		polyWarp(imageData, catalogData, WCS)
		
		totalResiduals.append(numDropped*MINWEIGHT*(MINWEIGHT_DOS**2) + sum(square(imageData['residuals'])))
		
		redist_countdown = redist_countdown - 1
		if (iter > 3) & all(abs(totalResiduals[-3:-1] - totalResiduals[-1]) <= MIN_IRLS_ACCURACY):
			if just_requeried:
				break
			else:
				redist_countdown = 0
	
	print 'image tweaked in', iter, 'iterations'

# This "adds" the affine warp in WCS.warpM to CD
def pushAffine2WCS(WCS):
	startCRPix = array([WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]])
	centerShift = array(WCS.warpM.reshape(2,-1)[:,-1].T)[0]
	linearWarp = WCS.warpM.reshape(2,-1)[:,-3:-1]

	(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = startCRPix - centerShift
	(WCS.wcstan.crval[0], WCS.wcstan.crval[1]) = WCS.pixelxy2radec(startCRPix[0], startCRPix[1])
	(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = startCRPix

	CD = matrix(WCS.wcstan.cd[:]).reshape(2,2)
	CD2 = (CD*linearWarp).reshape(-1,1)
	for i in arange(0,4):
		WCS.wcstan.cd[i] = CD2[i]
	
	# Flush warp out of WCS.warpM
	WCS.warpM.reshape(2,-1)[:,-3:] = mat([1., 0., 0. , 0., 1., 0.]).reshape(2,3)

# This *replaces* the SIP warp in the WCS with the higher-order terms in WCS.warpM
def pushPoly2WCS(WCS):
	SIP_im2cat = WCS.warpM.reshape(2,-1)[:,:-3].reshape(-1,1).copy()
	
	interval = 0.025
	count = int(1/interval)+1
	# gridBase = 1.0 - 2.0*matrix(arange(0, 1+interval, interval)).T	
	# gridStart = concatenate((repeat( (WCS.wcstan.imagew-WCS.wcstan.crpix[0]) * gridBase, count, 0), tile( (WCS.wcstan.imageh-WCS.wcstan.crpix[1]) * gridBase, (count,1))), 1)
	
	gridBase = matrix(arange(0, 1+interval, interval)).T	
	gridStart = concatenate((repeat( gridBase * WCS.wcstan.imagew - WCS.wcstan.crpix[0], count, 0), tile( gridBase * WCS.wcstan.imageh - WCS.wcstan.crpix[1], (count,1))), 1)

	gridEnd = gridStart + (polyExpand(gridStart, WCS.a_order,WCS.ab_order_min)*SIP_im2cat).reshape(-1,2)
	
	SIP_cat2im = linalg.lstsq(polyExpand(gridStart, WCS.ap_order, WCS.abp_order_min),(gridStart-gridEnd).reshape(-1,1))[0]
	gridStart_approx = gridEnd + (polyExpand(gridEnd, WCS.ap_order, WCS.abp_order_min)*SIP_cat2im).reshape(-1,2)
	
	print 'warp inversion error:', sum(abs(gridStart_approx - gridStart),0)/gridStart.shape[0]
	
	col = 0
	for d in arange(WCS.a_order, WCS.ab_order_min-1, -1):
		for i in arange(0, d+1):
			idx = (d-i)*10+i
			WCS.a[idx] = SIP_im2cat[col]
			WCS.b[idx] = SIP_im2cat[col+SIP_im2cat.shape[0]/2]
			col = col + 1

	col = 0
	for d in arange(WCS.ap_order, WCS.abp_order_min-1, -1):
		for i in arange(0, d+1):
			idx = (d-i)*10+i
			WCS.ap[idx] = SIP_cat2im[col]
			WCS.bp[idx] = SIP_cat2im[col+SIP_cat2im.shape[0]/2]
			col = col + 1
	
	# Flush polynomial warp out of warpM
	WCS.warpM.reshape(2,-1)[:,:-3] = 0


def writeOutput(WCS, inputWCSFilename, outputWCSFilename, catalogXYFilename, catalogRDFilename, imageXYFilename, imageRDFilename, renderOutput):
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
				imageXYData = loadFITS(imageXYFilename, ['X', 'Y'])[0]
				renderCatalogImage(catalogXYData, imageXYData, WCS_out)
				title('Fit (TAN+SIP)')
				savefig('5_TAN_SIP.png')
			else:
				print 'not rendering warped catalog because catalog output not specified'
	
			if imageRDFilename != ():
				imageRDData = loadFITS(imageRDFilename, ['RA', 'DEC'])[0]
				catalogRDData = loadFITS(catalogRDFilename, ['RA', 'DEC'])[0]
				renderCatalogImageRADec(catalogRDData, imageRDData, WCS_out)
				title('Fit (TAN+SIP) on Sphere')
				savefig('6_TAN_SIP_sphere.png')
			else:
				print 'not rendering warped image because image output not specified'
	else:
		print 'cannot write WCS output, all output aborted'
