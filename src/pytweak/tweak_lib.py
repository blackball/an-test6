from pylab import *
from numpy import *
from constants import *
import pyfits
import util.sip as sip
import os
import pdb

class RankError(Exception):
	def __init__(self, rank, neworder):
		self.rank = rank
		self.neworder = neworder
	def __str__(self):
		return 'rank of', self.rank, 'not sufficient, try using order=', self.neworder 

####### UTILITIES ############################################################

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

####### LOADING FILES ########################################################

def loadFITS(filename, fields):
	FITS = pyfits.open(filename)
	data = {}
	for f in fields:
		data[f] = double(FITS[1].data.field(f))
	return (data, FITS)


def loadData(imageXYFilename, catalogRDFilename, inputWCSFilename, goalOrder):
	imageData = loadFITS(imageXYFilename, ['X', 'Y', 'FLUX', 'BACKGROUND'])[0]
	
	# Fudge factor until we get the real data
	imageData['ERRS'] = 0*imageData['X'] + 1.0
	imageData['RADII'] = 0*imageData['X'] + 1.0
	
	catalogData = loadFITS(catalogRDFilename, ['RA', 'DEC'])[0]
	# These two are also fudged:
	catalogData['SIGMA_X'] = 0*catalogData['RA'] + 1.0
	catalogData['SIGMA_Y'] = 0*catalogData['RA'] + 1.0
	
	WCS = sip.Sip(inputWCSFilename)
	
	if any(array(WCS.a)!=0) | any(array(WCS.b)!=0) | any(array(WCS.ap)!=0) | any(array(WCS.bp)!=0) | (WCS.a_order!=0) | (WCS.b_order!=0) | (WCS.ap_order!=0) | (WCS.bp_order!=0):
		print 'warning: WCS already has SIP components, ignoring them.'
		for i in range(0,100):
			WCS.a[i] = 0.
			WCS.b[i] = 0.
			WCS.ap[i] = 0.
			WCS.bp[i] = 0.
		WCS.a_order=0
		WCS.b_order=0
		WCS.ap_order=0
		WCS.bp_order=0
	
	WCS.goalOrder = goalOrder;
	WCS.order = goalOrder;
	
	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
	
	# This constructs a rough "maximum" distance we should search in the 
	# image plane.
	minSize = array([catalogData['SIGMA_X'].shape[0], imageData['ERRS'].shape[0]]).min()
	maxSigmas = mean(sqrt(column_stack((imageData['ERRS'][1:minSize], imageData['ERRS'][1:minSize]))**2 + column_stack((catalogData['SIGMA_X'][1:minSize], catalogData['SIGMA_Y'][1:minSize]))**2),1)
	maxSigma = mean(maxSigmas) + 3*std(maxSigmas)
	imageData['maxImageDist'] = MAXDIST_MULT*sqrt(maxSigma*((WEIGHT_SIGMA**2)*(1-MINWEIGHT)/MINWEIGHT))
	
	return (imageData, catalogData, WCS)


###### TWEAKING ##############################################################

def fixCRPix(imageData, catalogData, WCS, goal_crpix, progressiveWarp=False, renderOutput=False):
	print "fixing crpix"
	
	# WCS.order = 1
	# Calling this does not tweak the image, just gets the correspondences
	tweakImage(imageData, catalogData, WCS)#, True)
		
	(WCS.wcstan.crval[0], WCS.wcstan.crval[1]) = WCS.pixelxy2radec(goal_crpix[0], goal_crpix[1])
	(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = (goal_crpix[0], goal_crpix[1])
	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
	
	# We probably wouldn't have to do this if we could rotate the CD matrix
	# correctly, to account for the change in RA (right?)
	pushAffine2WCS(imageData, catalogData, WCS)


def findAllPairs(a, b, maxDist):
	distSqMat = tile(mat(sum(square(a),1)).T, (1,b.shape[0])) + tile(mat(sum(square(b),1)), (a.shape[0],1)) - 2*(mat(a)*mat(b).T)
	distSqMat[distSqMat<0.0] = 0.0
	## Identical to Matlab, useful for debugging
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
	
	# I have no idea if the d_ or dd_ vars are set correctly. They only 
	# exist for eventually extending this to do Blind-Date.
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

def getMaxOrder(rank):
	return int(floor((-3. + sqrt(3.*3. - 4.*(2.-rank)))/2.))

def getFreedom(D, Dstart = 0):
	# return ((D*(D+3)) - (Dstart*(Dstart+1))) + 2.
	return (D+1)*(D+2) - (Dstart)*(Dstart+1)

def polyExpand(imagePoints, D, Dstart = 0):
	x = imagePoints[:,0]
	y = imagePoints[:,1]
	
	# width = (((D*(D+3)) - (Dstart*(Dstart+1))) + 2.)/2.
	width = getFreedom(D, Dstart)
	halfwidth = width/2.
	
	A_sub = double(mat(zeros((x.shape[0], halfwidth))))
	
	col = 0
	for d in arange(D, Dstart-1, -1):
		for i in arange(0, d+1):
			A_sub[:,col] = multiply(power(x,(d-i)), power(y,i))
			col = col + 1;
	
	A = zeros((2*x.shape[0], width))
	A[::2,0:halfwidth] = A_sub
	A[1::2,halfwidth:width] = A_sub
	return A

# def findWarp_robust(imageData, catalogData, WCS):
# 	WCS.order = WCS.goalOrder
# 	while True:
# 		try:
# 			findWarp(imageData, catalogData, WCS)
# 			break
# 		except RankError, e:
# 			print 'switching order to', e.neworder
# 			WCS.order = e.neworder


def findWarp(imageData, catalogData, WCS):
	crpix = WCS.wcstan.crpix
	pairs = imageData['pairs']
	
	imagePoints_all = matrix(column_stack((imageData['X'] - crpix[0], imageData['Y'] - crpix[1])))
	imagePoints = imagePoints_all[pairs[:,0]]
	catalogPoints = matrix(column_stack((catalogData['X'][pairs[:,1]] - crpix[0], catalogData['Y'][pairs[:,1]] - crpix[1])))
	warpWeights = matrix(sqrt(imageData['weights'])/imageData['sigmas']).T
	
	if warpWeights.shape[1] < 2:
		warpWeights_double = tile(warpWeights, (1,2)).reshape(-1,1)
	else:
		warpWeights_double = warpWeights.reshape(-1,1)
	
	b = catalogPoints.reshape(-1,1)
	A = polyExpand(imagePoints, WCS.order)
	A_all = polyExpand(imagePoints_all, WCS.order)
	
	WA = multiply(warpWeights_double.repeat(A.shape[1],1), A)
	Wb = multiply(warpWeights_double, b)
	(M, junk1, rank, junk2) = linalg.lstsq(WA,Wb)
	resid = (Wb - WA*M)
	
	if rank < WA.shape[1]:
		raise RankError(rank, getMaxOrder(rank))
	
	imagePoints_warp = (A_all * M).reshape(-1,2)
	
	imageData['X_WARP'] = array(imagePoints_warp[:,0])[:,0] + crpix[0]
	imageData['Y_WARP'] = array(imagePoints_warp[:,1])[:,0] + crpix[1]
	imageData['residuals'] = resid
	WCS.warpM = M


# def tweakImage_progressive(imageData, catalogData, WCS):
# 	for deg in arange(1, WCS.goalOrder+1):
# 		WCS.order = deg
# 		tweakImage(imageData, catalogData, WCS)


def tweakImage(imageData, catalogData, WCS, onlyCorrespondences=False):
	
	imageData['X_WARP'] = imageData['X']
	imageData['Y_WARP'] = imageData['Y']
	
	numAllPairs = imageData['X'].shape[0]*catalogData['X'].shape[0]
	
	totalResiduals = []
	redist_countdown = 0
	
	if onlyCorrespondences:
		print 'finding weighted correspondences'
	else:
		print 'tweaking image with order ' + str(WCS.order) + '...', 
	
	for iter in range(1,1+TWEAK_MAX_NUM_ITERS):
		
		just_requeried = 0;
		if redist_countdown == 0:
			(pairs_all, dists_all) = findAllPairs(column_stack((imageData['X_WARP'], imageData['Y_WARP'])), column_stack((catalogData['X'], catalogData['Y'])), imageData['maxImageDist'])
			sigmas_all = (sqrt(imageData['ERRS'][pairs_all[:,0]]**2 + catalogData['SIGMA_X'][pairs_all[:,1]]**2) + sqrt(imageData['ERRS'][pairs_all[:,0]]**2 + catalogData['SIGMA_Y'][pairs_all[:,1]]**2))/2
			just_requeried = 1
			redist_countdown = REDIST_INTERVAL
		else:
			dists_all = sqrt(square(imageData['X_WARP'][pairs_all[:,0]] - catalogData['X'][pairs_all[:,1]]) + square(imageData['Y_WARP'][pairs_all[:,0]] - catalogData['Y'][pairs_all[:,1]]))
		
		weights_all = getWeight(dists_all, sigmas_all)[0]

		toKeep = weights_all > MINWEIGHT
		numDropped = numAllPairs - sum(toKeep)
		imageData['pairs'] = pairs_all[toKeep,:]
		imageData['sigmas'] = sigmas_all[toKeep,:]
		imageData['dists'] = dists_all[toKeep,:]
		imageData['weights'] = weights_all[toKeep,:]
		
		if onlyCorrespondences:
			return
		
		findWarp(imageData, catalogData, WCS)
		
		totalResiduals.append(numDropped*MINWEIGHT*(MINWEIGHT_DOS**2) + sum(square(imageData['residuals'])))
		
		redist_countdown = redist_countdown - 1
		if (iter > 3) & all(abs(totalResiduals[-3:-1] - totalResiduals[-1]) <= MIN_IRLS_ACCURACY):
			if just_requeried:
				break
			else:
				redist_countdown = 0
	
	print 'tweaked in', iter, 'iterations'


####### WCS MANIPULATION #####################################################

# This can be called after correspondences have been found in tweakImage
def pushAffine2WCS(imageData, catalogData, WCS):
	shiftAmount_last = Inf
	for iter in range(1, MAX_CAMERA_ITERS):
		findWarp(imageData, catalogData, WCS)	
		shiftAmount = sqrt(sum(square(array(WCS.warpM.reshape(2,-1)[:,-1].T)[0])))
		pushShift2WCS(WCS)
		(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
		
		if shiftAmount >= shiftAmount_last:
			break
		shiftAmount_last = shiftAmount
	
	findWarp(imageData, catalogData, WCS)
	pushLinear2WCS(WCS)
	(catalogData['X'], catalogData['Y']) = WCS_rd2xy(WCS, catalogData['RA'], catalogData['DEC'])
	
	# This is to check that this iterative process is enough.
	# We can drop these lines when we're confident
	findWarp(imageData, catalogData, WCS)
	assert(all(WCS.warpM.reshape(2,-1)[:,-1] < MAX_SHIFT_AMOUNT))
	assert(all(abs((WCS.warpM.reshape(2,-1)[:,-3:-1] - matrix([1., 0., 0., 1.]).reshape(2,2))) < MAX_LWARP_AMOUNT))


# This *adds* the shift in WCS.warpM to CRVal
def pushShift2WCS(WCS):
	startCRPix = array([WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]])	
	startCRVal = array((WCS.wcstan.crval[0], WCS.wcstan.crval[1]))
	centerShift = array(WCS.warpM.reshape(2,-1)[:,-1].T)[0]
	(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = startCRPix - centerShift
	newCRVal = array(WCS.pixelxy2radec(startCRPix[0], startCRPix[1]))
	
	(WCS.wcstan.crpix[0], WCS.wcstan.crpix[1]) = startCRPix
	(WCS.wcstan.crval[0], WCS.wcstan.crval[1]) = newCRVal
	
	WCS.warpM.reshape(2,-1)[:,-1] = mat([0. , 0.]).T

# This *multiplies* the warp in CD by the linear component of WCS.warpM
def pushLinear2WCS(WCS):
	linearWarp = WCS.warpM.reshape(2,-1)[:,-3:-1]
	
	CD = matrix(WCS.wcstan.cd[:]).reshape(2,2)
	CD2 = (CD*linearWarp).reshape(-1,1)
	for i in arange(0,4):
		WCS.wcstan.cd[i] = CD2[i]
	
	WCS.warpM.reshape(2,-1)[:,-3:-1] = mat([1., 0., 0., 1.]).reshape(2,2)

# This *replaces* the SIP warp in the WCS with the higher-order terms in WCS.warpM
# Here we also construct the inverse of the SIP warp.
def pushPoly2WCS(WCS):
	
	# If the warp doesn't include higher order components, the SIP parts of
	# the WCS should look as empty as possible (ideally, they shouldn't appear
	# at all)
	if WCS.order <= 1:
		WCS.a_order = 0
		WCS.b_order = 0
		WCS.ab_order_min = 0
		WCS.ap_order = 0
		WCS.bp_order = 0
		WCS.abp_order_min = 0
		for i in range(0,100):
			WCS.a[i] = 0.
			WCS.b[i] = 0.
			WCS.ap[i] = 0.
			WCS.bp[i] = 0.
		return
	
	# We dump the image->catalog warp (which we directly computed) into SIP
	WCS.a_order = WCS.order
	WCS.b_order = WCS.order	
	WCS.ab_order_min = AB_ORDER_MIN
	
	SIP_im2cat = WCS.warpM.reshape(2,-1)[:,:-3].reshape(-1,1).copy()
	col = 0
	for d in arange(WCS.a_order, WCS.ab_order_min-1, -1):
		for i in arange(0, d+1):
			idx = (d-i)*10+i
			WCS.a[idx] = SIP_im2cat[col]
			WCS.b[idx] = SIP_im2cat[col+SIP_im2cat.shape[0]/2]
			col = col + 1
	
	# We compute the catalog->image warp seperately
	WCS.ap_order = WCS.order
	WCS.bp_order = WCS.order
	WCS.abp_order_min = ABP_ORDER_MIN
	
	count = 50
	print 'inverting warp with', count, 'by', count, 'grid'
	
	gridBase = matrix(arange(0, count)/float(count-1)).T
	gridStart = concatenate((repeat( gridBase * WCS.wcstan.imagew - WCS.wcstan.crpix[0], count, 0), tile( gridBase * WCS.wcstan.imageh - WCS.wcstan.crpix[1], (count,1))), 1)
	
	
	gridEnd = gridStart + (polyExpand(gridStart, WCS.a_order,WCS.ab_order_min)*SIP_im2cat).reshape(-1,2)
	lstsq = linalg.lstsq(polyExpand(gridEnd, WCS.ap_order, WCS.abp_order_min),(gridStart-gridEnd).reshape(-1,1))
	SIP_cat2im = lstsq[0]
	resids = lstsq[1]/gridEnd.shape[0]
	print "warp inversion avg residual =", resids[0]
	
	# gridStart_approx = gridEnd + (polyExpand(gridEnd, WCS.ap_order, WCS.abp_order_min)*SIP_cat2im).reshape(-1,2)
	# figure()
	# scatter(gridStart[:,0], gridStart[:,1], marker = 'o', s=40, facecolor=(1,1,1,0), edgecolor=COLOR2)
	# scatter(gridStart_approx[:,0], gridStart_approx[:,1], marker = 'o', s=20, facecolor=COLOR1, edgecolor=(1,1,1,0))
	# scatter(array([WCS.wcstan.crpix[0]]), array([WCS.wcstan.crpix[1]]), marker = '^', s=200, facecolor=(0.3,1,0.5,0.3), edgecolor=(0,0,0,0.5))
	# axis('equal')
	# axis((min(gridStart[:,0]), max(gridStart[:,0]), min(gridStart[:,1]), max(gridStart[:,1])))
	# legend(('start', 'approx'))
	# print 'warp inversion error:', sum(abs(gridStart_approx - gridStart),0)/gridStart.shape[0]
	
	# We dump the inverse warp into the ap/bp component of the SIP
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
			if comment[0:5] != 'Tweak':
				WCSFITS_new[0].header.add_comment(comment)
		
		WCSFITS_new[0].header.add_comment('Tweak: yes')
		WCSFITS_new[0].header.add_comment('Tweak AB order: ' + str(WCS.a_order))
		WCSFITS_new[0].header.add_comment('Tweak ABP order: ' + str(WCS.ap_order))
		
		if WCSFITS_old[0].header.get('AN_JOBID') != None:
			WCSFITS_new[0].header.add_comment('AN_JOBID: ' + WCSFITS_old[0].header.get('AN_JOBID'))
		if WCSFITS_old[0].header.get('DATE') != None:
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
				savefig('4_TAN_SIP.png')
			else:
				print 'not rendering warped catalog because catalog output not specified'
	
			if imageRDFilename != ():
				imageRDData = loadFITS(imageRDFilename, ['RA', 'DEC'])[0]
				catalogRDData = loadFITS(catalogRDFilename, ['RA', 'DEC'])[0]
				renderCatalogImageRADec(catalogRDData, imageRDData, WCS_out)
				title('Fit (TAN+SIP) on Sphere')
				savefig('5_TAN_SIP_sphere.png')
			else:
				print 'not rendering warped image because image output not specified'
	else:
		print 'cannot write WCS output, all output aborted'



####### RENDERING ############################################################

def renderCatalogImage(catalogData, imageData, WCS):
	figure()
	scatter(imageData['X'], imageData['Y'], marker = 'o', s=40, facecolor=(1,1,1,0), edgecolor=COLOR2)
	scatter(catalogData['X'], catalogData['Y'], marker = 'o', s=20, facecolor=COLOR1, edgecolor=(1,1,1,0))
	scatter(array([WCS.wcstan.crpix[0]]), array([WCS.wcstan.crpix[1]]), marker = '^', s=200, facecolor=(0.3,1,0.5,0.3), edgecolor=(0,0,0,0.5))
	axis('equal')
	try:
		axis((min(imageData['X']), max(imageData['X']), min(imageData['Y']), max(imageData['Y'])))
	except:
		axis((min(imageData['X_WARP']), max(imageData['X_WARP']), min(imageData['Y_WARP']), max(imageData['Y_WARP'])))
	legend(('image star', 'catalog star'))

def renderCatalogImageRADec(catalogData, imageData, WCS):
	figure()
	scatter(imageData['RA'], imageData['DEC'], marker = 'o', s=40, facecolor=(1,1,1,0), edgecolor=COLOR2)
	scatter(catalogData['RA'], catalogData['DEC'], marker = 'o', s=20, facecolor=COLOR1, edgecolor=(1,1,1,0))
	scatter(array([WCS.wcstan.crval[0]]), array([WCS.wcstan.crval[1]]), marker = '^', s=200, facecolor=(0.3,1,0.5,0.3), edgecolor=(0,0,0,0.5))
	axis('image')
	legend(('image star', 'catalog star'))

def renderImageMotion(imageData, WCS):
	figure()
	scatter(imageData['X_WARP'], imageData['Y_WARP'], marker = 'o', s=40, facecolor=(1,1,1,0), edgecolor=COLOR2)
	scatter(imageData['X'], imageData['Y'], marker = 'o', s=20, facecolor=(1,1,1,0), edgecolor=COLOR1)
	scatter(array([WCS.wcstan.crpix[0]]), array([WCS.wcstan.crpix[1]]), marker = '^', s=200, facecolor=(0.3,1,0.5,0.3), edgecolor=(0,0,0,0.5))
	axis('image')
	axis((min(imageData['X']), max(imageData['X']), min(imageData['Y']), max(imageData['Y'])))
	legend(('image star', 'catalog star'))


####### BONUS STUFF ##########################################################

# It's pretty silly that this has to be here, but it needs constants.py
# and refers to getWeight(), and doesn't make sense to include in a function
MINWEIGHT = (getWeight(MINWEIGHT_DOS,1))[0]
MAXERROR = MINWEIGHT*(MINWEIGHT_DOS**2);
