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

rads = sum((fxy[keepers[:,0]] - A)**2,1)**.5
dists = sum((fxy[keepers[:,0]] - ixy[keepers[:,1]])**2,1)**.5
figure()
plot(rads, dists, 'r.')
ylabel('r_field - r_ind where r is distance from quad center')
xlabel('distance from field object to quad center')
show()

def fitpoly1d(x, y, order):
    A = vstack(x**(2*i) for i in range(order)).T
    return linalg.lstsq(A, y)

def genpoly1d(x,ll):
    A = vstack(x**(2*i) for i in range(len(ll))).T
    return dot(A, ll)

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
    lll,resids,rank,s = fitpoly1d(rads[minfitpts], dists[minfitpts], order)
    trydists = genpoly1d(rads, lll)
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
lll_best,resids,rank,s = fitpoly1d(rads[inlier_pts_best], dists[inlier_pts_best], order)

plot(rads,dists,'r.', label='distances')
a = axis()
t=arange(1.,700.,20)
plot(t,genpoly1d(t, lll_best),'g--',linewidth=0.3, label='fitted radial distortion')
legend()
axis(a)

"""
MAXIMA CODE:

    This converts from the radial distortion terms in r^2 etc into SIP
    distortion terms a_pq * x^p * y^q, when the center of distortion is not the
    center of the coordinate system.

    After running this you have to copy the generated coefficients into the
    code. ick!

r : sqrt((x-cx)^2 + (y-cy)^2);
distortion_factor : (1 + a1*r^2 + a2*r^4 + a3*r^6); 
distortion_factor : (1 + a1*r^2); 
distortion_factor : (1 + a1*r^2 + a2*r^4); 
distortion_factor : expand(distortion_factor);
x_distorted : expand(x * distortion_factor + cx);
y_distorted : expand(y * distortion_factor + cy);
for p:0 thru 4 do
    for q:0 thru 4 do (
        a_pq : coeff(coeff(x_distorted, x, p), y, q),
        print(p, q, string(a_pq))
    );

"""
