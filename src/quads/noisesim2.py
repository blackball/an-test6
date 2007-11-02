#from numpy import array, matrix, linalg
from numpy import *
from numpy.random import *
from numpy.linalg import *
from matplotlib.pylab import figure, plot

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
    print 'mean(X) is\n', mx
    print 'mean(Y) is\n', my
    T.incenter = mx
    T.outcenter = my

    #print 'X-mx is\n', X-mx
    #print '(X-mx)^2 is\n', (X-mx)*(X-mx)
    varx = sum(sum((X - mx)*(X - mx)), axis=1)
    vary = sum(sum((Y - my)*(Y - my)), axis=1)
    #print 'var(X) is', varx
    #print 'var(Y) is', vary
    T.scale = sqrt(vary / varx)
    print 'scale is', T.scale

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
    print 'R is\n', R
    T.rotation = R
    return T


if __name__ == '__main__':

    inoise = 1
    fnoise = 0
    iqnoise = -1
    dimquads = 4
    N = 1000
    quadscale = 100
    imgsize = 1000

    #T = Transform()
    #T.scale = 2
    #T.rotation = matrix([[0, 1], [1,0]])
    #T.incenter = array([[3,4]]).transpose()
    #T.outcenter = array([[8,9]]).transpose()
    #x = array([[3,5]]).transpose()
    #T.apply(x)
    
    fquad = zeros((2,dimquads))

    fquad[0,0] = imgsize/2 - quadscale/2
    fquad[1,0] = imgsize/2
    fquad[0,1] = imgsize/2 + quadscale/2
    fquad[1,1] = imgsize/2
    for i in range(2, dimquads):
        fquad[0,i] = imgsize/2 + randn(1) * quadscale
        fquad[1,i] = imgsize/2 + randn(1) * quadscale

    iquad = fquad + randn(*fquad.shape)

    T = procrustes(iquad, fquad)

    itrans = zeros(fquad.shape)

    for i in range(dimquads):
        fq = fquad[:,i].reshape(2,1)
        #print 'fquad[i] is', fquad[:,i]
        #print 'T(fquad[i]) is', T.apply(fquad[:,i].transpose())
        #print 'fq = ', fq
        #print 'T(fq) = ', T.apply(fq)
        itrans[:,i] = T.apply(fq).transpose()

    print 'fquad is', fquad
    print 'iquad is', iquad
    print 'itrans is', itrans

    figure()
    I=[0,2,1,3,0];
    plot(fquad[0,I], fquad[1,I], 'bo-', itrans[0,I], itrans[1,I], 'ro-')

    fstars = rand((2,N)) * imgsize
    for i in range(dimquads):
    istars = 
