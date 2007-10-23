import sys
from numpy import where, linalg, arange, vstack, hstack, dot
from numpy.random import rand, randint

def robust_poly_fit(x,y, order=2, thresh=10, iterations=10, verbose=False):
    npts = order
    n = len(x)
    assert len(x) == len(y)
    for iteration in range(iterations):
        minfitpts = [randint(0,n-1) for i in range(npts)]
        coeffs,resids,rank,s = fitpoly1d(x[minfitpts], y[minfitpts], order)
        yprime = genpoly1d(x, coeffs)
        err = (yprime - y)**2
        num_inliers = sum(err < thresh)
        # hybrid inlier / sqd err 'score' makes this the MSAC variant of RANSAC
        score = sum(err[err < thresh]) + sum(err >= thresh) * thresh
        if iteration == 0 or score < best_score:
            coeffs_best = coeffs
            best_score = score
            inlier_pts_best = where(err < thresh)[0]
            if verbose:
                print '%04d %04d %10f' % (iteration, num_inliers, best_score), coeffs
                sys.stdout.flush()

    coeffs_best,resids,rank,s = fitpoly1d(x[inlier_pts_best],
                                          y[inlier_pts_best], order)
    return coeffs_best, inlier_pts_best, resids

def fitpoly1d(x, y, order):
    AA = vstack(x**i for i in range(order)).T
    return linalg.lstsq(AA, y)

def genpoly1d(x, coeffs):
    AA = vstack(x**i for i in range(len(coeffs))).T
    return dot(AA,  coeffs)

# look ma! tests!!!

def close(a,b,eps=1e-4):
    return abs(a-b) < eps

def test_no_outliers():
    x = arange(1,100)
    y = 1 + 2*x**2
    theta,inliers,resids = robust_poly_fit(x, y, order=3, thresh=10, iterations=10)
    assert close(theta[0], 1.)
    assert close(theta[1], 0.)
    assert close(theta[2], 2.)

def test_half_outliers():
    N = 100
    x = arange(N)
    y = 1 + 2*x**2
    x = hstack((x, x))
    y = hstack((y, N**2*rand(N)))
    theta,inliers,resids = robust_poly_fit(x, y, order=3, thresh=10,
                                           iterations=50)
    assert close(theta[0], 1.)
    assert close(theta[1], 0.)
    assert close(theta[2], 2.)
