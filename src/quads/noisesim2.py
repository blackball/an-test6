#from numpy import array, matrix, linalg
from numpy import *
from numpy.random import *
from numpy.linalg import *
from matplotlib.pylab import figure, plot, xlabel, ylabel, loglog, clf
#from pylab import *

class Transform(object):
    scale = None
    rotation = None
    incenter = None
    outcenter = None

    def apply(self, X):
        #print X
        dx = X - self.incenter
        #print dx
        dx = dx * self.scale
        #print dx
        dx = self.rotation * dx
        #print dx
        dx = dx + self.outcenter
        #print dx
        return dx


def procrustes(X, Y):
    T = Transform()
    sx = X.shape
    if sx[0] != 2:
        print 'X must be 2xN'
    sy = Y.shape
    if sy[0] != 2:
        print 'Y must be 2xN'
    N = sx[1]

    mx = X.mean(axis=1).reshape(2,1)
    my = Y.mean(axis=1).reshape(2,1)
    #print 'mean(X) is\n', mx
    #print 'mean(Y) is\n', my
    T.incenter = mx
    T.outcenter = my

    #print 'X-mx is\n', X-mx
    #print '(X-mx)^2 is\n', (X-mx)*(X-mx)
    varx = sum(sum((X - mx)*(X - mx)), axis=1)
    vary = sum(sum((Y - my)*(Y - my)), axis=1)
    #print 'var(X) is', varx
    #print 'var(Y) is', vary
    T.scale = sqrt(vary / varx)
    #print 'scale is', T.scale

    C = zeros((2,2))
    for i in [0,1]:
        for j in [0,1]:
            C[i,j] = sum((X[i,:] - mx[i]) * (Y[j,:] - my[j]))
    #print 'cov is\n', C

    U,S,V = svd(C)
    U = matrix(U)
    V = matrix(V)
    
    #print 'U is\n', U
    #print 'U\' is\n', U.transpose()
    #print 'V is\n', V
    R = V * U.transpose()
    #print 'R is\n', R
    T.rotation = R
    return T


def draw_sample(inoise=1, fnoise=0, iqnoise=-1,
                dimquads=4, quadscale=100, imgsize=1000,
                Rsteps=10, Asteps=36):

    # Stars that compose the field quad.
    fquad = zeros((2,dimquads))
    fquad[0,0] = imgsize/2 - quadscale/2
    fquad[1,0] = imgsize/2
    fquad[0,1] = imgsize/2 + quadscale/2
    fquad[1,1] = imgsize/2
    for i in range(2, dimquads):
        fquad[0,i] = imgsize/2 + randn(1) * quadscale
        fquad[1,i] = imgsize/2 + randn(1) * quadscale

    # Index quad is field quad plus jitter.
    iquad = fquad + randn(*fquad.shape)

    # Solve for transformation
    T = procrustes(iquad, fquad)

    # Put the index quad stars through the transformation
    itrans = zeros(fquad.shape)
    for i in range(dimquads):
        fq = fquad[:,i].reshape(2,1)
        itrans[:,i] = T.apply(fq).transpose()

    # Field quad center...
    qc = mean(fquad, axis=1)

    # Sample stars on a R^2, theta grid.
    #rads = sqrt((array(range(Rsteps))+1) / float(Rsteps)) * imgsize/2
    N = Rsteps * Asteps
    rads = sqrt((array(range(Rsteps))+0.5) / float(Rsteps)) * imgsize/2
    thetas = array(range(Asteps)) / float(Asteps) * 2.0 * pi
    fstars = zeros((2,N))
    for r in range(Rsteps):
        for a in range(Asteps):
            fstars[0, r*Asteps + a] = sin(thetas[a]) * rads[r] + qc[0]
            fstars[1, r*Asteps + a] = cos(thetas[a]) * rads[r] + qc[1]
    # Put them through the transformation...
    istars = zeros((2,N))
    for i in range(N):
        fs = fstars[:,i].reshape(2,1)
        istars[:,i] = T.apply(fs).transpose()

    R = sqrt((fstars[0,:] - qc[0])**2 + (fstars[1,:] - qc[1])**2)
    E = sqrt(sum((fstars - istars)**2, axis=0))

    # Fit to a linear model...
    xfit = R**2
    yfit = E**2
    A = zeros((2,N))
    A[0,:] = 1
    A[1,:] = xfit.transpose()
    (C,resids,rank,s) = lstsq(A.transpose(), yfit)

    return (fquad, iquad, T, itrans, qc, fstars, istars,
            R, E, C)

if __name__ == '__main__':

    N = 100
    C = zeros((2,N))
    QD = zeros((N))
    for i in range(N):
        (fquad, iquad, T, itrans, qc, fstars, istars, R, E, c) = draw_sample()
        C[:,i] = c
        QD[i] = sum((iquad - fquad)**2)

    C0 = C[0,:]
    C1 = C[1,:]

    figure(1)
    clf()
    loglog(C0, C1, 'ro')
    xlabel('C0')
    ylabel('C1')

    figure(2)
    clf()
    plot(QD, C1, 'bo')
    xlabel('Quad Distance')
    ylabel('C1')
    
    #figure(1)
    #I=[0,2,1,3,0];
    #plot(fquad[0,I], fquad[1,I], 'bo-', itrans[0,I], itrans[1,I], 'ro-')

    #figure(2)
    #plot(fstars[0,:], fstars[1,:], 'b.', istars[0,:], istars[1,:], 'r.')

    #figure(1)
    #I=[0,2,1,3,0];
    #plot(fquad[0,I], fquad[1,I], 'bo-',
    #     itrans[0,I], itrans[1,I], 'ro-',
    #     fstars[0,:], fstars[1,:], 'b.',
    #     istars[0,:], istars[1,:], 'r.')

    #figure(2)
    #plot(R, E, 'r.')
    #xlabel('R')
    #ylabel('E')

    #figure(3)
    #plot(R**2, E**2, 'r.')
    #xlabel('R^2')
    #ylabel('E^2')

    #print 'Fit coefficients are', C

    #figure(2)
    #xplot = array(range(101)) / 100.0 * max(xfit)
    #plot(R**2, E**2, 'r.',
    #     xplot, C[0] + C[1]*xplot, 'b-')
    #xlabel('R^2')
    #ylabel('E^2')

    #show()

