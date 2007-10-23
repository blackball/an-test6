import sip
import pyfits
from pylab import *
from numpy import *
import ransac

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
cutoff = sigma * 50

keepers = []
for nf in range(len(fxy)):
    rad = fxy[nf] - quadcenter
    r1, r2 = rad/norm(rad)
    factor = 5.0
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

radsf = sum((fxykeep - quadcenter)**2,1)**.5
radsi = sum((ixykeep - quadcenter)**2,1)**.5

################################################################################
figure()
plot(fxykeep[:,0], fxykeep[:,1], 'gx', label='field')
plot(ixykeep[:,0], ixykeep[:,1], 'r+', label='index')
legend()
title('stars which were found in correspondence using prior on covariance')

################################################################################
# DIRECT RATIO ESTIMATE
# fit ri/rf; need a multiplicitave update
figure()
t=arange(1.,700.,20)
t **= 2
plot(t, ones(len(t)), 'b--',linewidth=0.3, label='r^2=r^2 line')
ratio = radsi/radsf
ratio[~isfinite(ratio)] = 1.0
radsf2 = radsf**2
coeffs,inliers,resids = ransac.robust_poly_fit(
    radsf2, ratio, order=3, thresh=0.0001, iterations=500, verbose=True)

plot(radsf2, ratio, 'r.')
xlabel('r_field^2')
ylabel('r_index / r_field')

a = axis()
plot(t,ransac.genpoly1d(t, coeffs),'g--',linewidth=0.3, label='fitted radial distortion ratio')
plot(radsf2[inliers], ratio[inliers], 'b.', label='points used in fit')
legend()
axis(a)
title('ratio model')

def correct(xy, center, coeffs):
    return (xy-center)*c_[ransac.genpoly1d(sum((xy-center)**2,1), coeffs)] + center

################################################################################
figure()
scatter(ixy[:,0], ixy[:,1], 50, label='index', facecolor='#ffffff')
#plot(fxykeep[:,0], fxykeep[:,1], 'gx', label='field')
fxycorr = correct(fxykeep, quadcenter, coeffs)
plot(fxycorr[:,0],fxycorr[:,1], 'bx', label='field corrected')
fxycorr_inliers = correct(fxykeep[inliers], quadcenter, coeffs)
scatter(fxycorr_inliers[:,0],fxycorr_inliers[:,1], 50,
        label='inliers corrected',alpha=0.2,faceted=False)
#plot(ixykeep[:,0], ixykeep[:,1], 'r+', label='index')
#plot(ixy[:,0], ixy[:,1], 'r+', label='index')
plot([quadcenter[0]], [quadcenter[1]], 'ys', label='quad center')
legend()
title('stars which were found in correspondence using prior on covariance CORRECTED')
show()

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
