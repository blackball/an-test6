import sip
import pyfits
from pylab import *
from numpy import *

def readxylist(fn):
    hdus = pyfits.open(fn)
    table = hdus[1].data
    fx = table.field(0)
    fy = table.field(1)
    fxy = vstack((fx,fy))
    return fxy.T

fxy = readxylist('field1.xy.fits')
ixy = readxylist('index1.xy.fits')

origwcs = sip.Tan('wcs1.fits')
cx = origwcs.crpix[0]
cy = origwcs.crpix[1]
crpix = array((cx,cy), Float64)

# filter index stars to be in the image
ixy[all(ixy < fxy.max(0), 1)]

# grab matched quad 
hdus = pyfits.open('match1.fits')
table = hdus[1].data
quadpix = table.field('quadpix')[0].reshape(5,2)
A, B, C, D, bogus = quadpix
quadrad = 0.5*norm(A-B)
quadcenter = 0.5*(A+B)

# arcsec per pix = sqrt(abs(det(CD)))
pixscale = 97.7

sigma = hypot(1 / pixscale, 1)
cutoff = sigma * 30

keepers = []
for nf in range(len(fxy)):
    rad = fxy[nf] - quadcenter
    r1, r2 = rad/norm(rad)
    factor = 6.0
    covmat = array([[r1,r2],
                    [-factor*r2,factor*r1]])
    for ni in range(len(ixy)):
        dist = norm(dot(covmat, fxy[nf]-ixy[ni]))
        if dist < cutoff:
            print dist
            keepers.append((nf,ni))

keepers = array(keepers)
fxykeep = fxy[keepers[:,0]]
ixykeep = ixy[keepers[:,1]]
figure()
plot(fxykeep[:,0], fxykeep[:,1], 'gx', label='field')
plot(ixykeep[:,0], ixykeep[:,1], 'r+', label='index')
legend()
title('stars which were found in correspondence using prior on covariance')

radsf = sum((fxy[keepers[:,0]] - A)**2,1)**.5
dists = sum((fxy[keepers[:,0]] - ixy[keepers[:,1]])**2,1)**.5
figure()
plot(radsf, dists, 'r.')
ylabel('r_field - r_ind where r is distance from quad center')
xlabel('distance from field object to quad center')
show()

def fitpoly1d(x, y, order):
    AA = vstack(x**(2*i) for i in range(order)).T
    return linalg.lstsq(AA, y)

def genpoly1d(x,ll):
    AA = vstack(x**(2*i) for i in range(len(ll))).T
    return dot(AA, ll)

order = 3
npts = order
thresh = 3
thresh = 4
n = len(keepers)
# dumbass ransac without ANY EM; pure threshold
num_inliers_best = 0
best_score = 10000000000000000
for iteration in range(1000):
    minfitpts = [random.randint(0,n-1) for i in range(npts)]
    lll,resids,rank,s = fitpoly1d(radsf[minfitpts], dists[minfitpts], order)
    trydists = genpoly1d(radsf, lll)
    tryerrors = (trydists - dists)**2
    num_inliers = sum(tryerrors < thresh) # FIXME Make this some smart EM type thing
    # hybrid inlier / sqd err 'score' makes this the MSAC variant of RANSAC
    score = sum(tryerrors[tryerrors < thresh])+sum(tryerrors>=thresh)*thresh # FIXME Make this some smart EM type thing
    #if num_inliers > num_inliers_best:
    if score < best_score:
        lll_best = lll
        num_inliers_best = num_inliers
        best_score = score
        inlier_pts_best = find(tryerrors < thresh)
        print iteration, num_inliers_best, best_score, lll

# refit
lll_best,resids,rank,s = fitpoly1d(radsf[inlier_pts_best], dists[inlier_pts_best], order)

plot(radsf,dists,'r.', label='distances')
a = axis()
t=arange(1.,700.,20)
plot(t,genpoly1d(t, lll_best),'g--',linewidth=0.3, label='fitted radial distortion')
legend()
axis(a)

# alternate fit method: fit r directly rather than rf-ri
radsi = sum((ixy[keepers[:,1]] - A)**2,1)**.5
figure()
plot(radsf, radsi, 'r.')
plot(t, t, 'b--',linewidth=0.3, label='r=r line')
xlabel('distance from quad for field object')
ylabel('distance from quad for index object')

# alternate fit method: fit ri/rf instead; just need a multiplicitave update
figure()
plot(t, ones(len(t)), 'b--',linewidth=0.3, label='r=r line')
ratio = radsi/radsf
radsf_finite = radsf[isfinite(ratio)]
ratio_finite = ratio[isfinite(ratio)]
plot(radsf_finite, ratio_finite, 'r.')
xlabel('distance from quad for field object')
ylabel('ratio r_index / r_field')

# Copy N PASTE ORAMA!
order = 2
npts = order
thresh = 3
thresh = 0.0001 # in sqd error
n = len(ratio_finite)
# dumbass ransac without ANY EM; pure threshold
num_inliers_best = 0
best_score = 10000000000000000
for iteration in range(5000):
    minfitpts = [random.randint(0,n-1) for i in range(npts)]
    lll,resids,rank,s = fitpoly1d(radsf_finite[minfitpts], ratio_finite[minfitpts], order)
    tryratio_finite = genpoly1d(radsf_finite, lll)
    tryerrors = (tryratio_finite - ratio_finite)**2
    num_inliers = sum(tryerrors < thresh) # FIXME Make this some smart EM type thing
    # hybrid inlier / sqd err 'score' makes this the MSAC variant of RANSAC
    score = sum(tryerrors[tryerrors < thresh])+sum(tryerrors>=thresh)*thresh # FIXME Make this some smart EM type thing
    #if num_inliers > num_inliers_best:
    if score < best_score:
        lll_best = lll
        num_inliers_best = num_inliers
        best_score = score
        inlier_pts_best = find(tryerrors < thresh)
        print iteration, num_inliers_best, best_score, lll

# refit
lll_best,resids,rank,s = fitpoly1d(radsf_finite[inlier_pts_best], ratio_finite[inlier_pts_best], order)

a = axis()
t=arange(1.,700.,20)
plot(t,genpoly1d(t, lll_best),'g--',linewidth=0.3, label='fitted radial distortion ratio')
plot(radsf_finite[inlier_pts_best], ratio_finite[inlier_pts_best], 'b.',
     label='points used in fit')
legend()
axis(a)
title('ratio model')



"""
MAXIMA CODE:

    This converts from the radial distortion terms in r^2 etc into SIP
    distortion terms a_pq * x^p * y^q, when the center of distortion is not the
    center of the coordinate system.

    After running this you have to copy the generated coefficients into the
    code. ick!

INTERESTING NOTE:

    Fitting for rf - ri, implies adding the distortion, which oddly implies a
    division distortion model! oops. that can't map to SIP because SIP can't
    divide.

rf : sqrt((x-cx)^2 + (y-cy)^2);
distortion_correction : (a0 + a1*rf^2 + a2*rf^4 + a3*rf^6); 
distortion_correction : (a0 + a1*rf^2 + a2*rf^4); 
distortion_correction : (a0 + a1*rf^2); 
distortion_correction : expand(distortion_correction);
x_distorted : expand((x-cx)*distortion_correction + cx);
y_distorted : expand((y-cy)*distortion_correction + cy);
for p:0 thru 4 do
    for q:0 thru 4 do (
        a_pq : coeff(coeff(x_distorted, x, p), y, q),
        print(p, q, string(a_pq))
    );

"""
