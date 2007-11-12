import pyfits
from numpy import *
from matplotlib.pylab import figure, plot, xlabel, ylabel, loglog, clf
from matplotlib.pylab import semilogy, show, find

if __name__ == '__main__':

    ixy = pyfits.open('ver/index.xy.fits')
    ixy = ixy[1].data
    ix = ixy.field(0)
    iy = ixy.field(1)
    NI = len(ix)
    ixy = zeros((NI,2))
    ixy[:,0] = ix
    ixy[:,1] = iy

    fxy = pyfits.open('ver/field.xy.fits')
    fxy = fxy[1].data
    fx = fxy.field(0)
    fy = fxy.field(1)
    NF = len(fx)
    fxy = zeros((NF,2))
    fxy[:,0] = fx
    fxy[:,1] = fy

    mf = pyfits.open('ver/match.fits')
    mf = mf[1].data
    quad = mf.field('quadpix')[0]
    quad = quad[0:8].reshape(4,2)
    qx = quad[:,0]
    qy = quad[:,1]
    #qc = mean(quad, axis=0)
    #cx = qc[0]
    #cy = qc[1]
    cx = mean(qx)
    cy = mean(qy)

    iok = find( (ix > min(fx)) *
                (ix < max(fx)) *
                (iy > min(fy)) *
                (iy < max(fy)) )
    #ix = ix[iok]
    okix = []
    okiy = []
    for i in iok:
        okix.append(ix[i])
        okiy.append(iy[i])
    ix = okix
    iy = okiy

    figure(1)
    clf()
    I = [0, 2, 1, 3, 0]
    plot(
        [cx], [cy], 'ro',
        qx[I], qy[I], 'r-',
        ix, iy, 'rx',
        fx, fy, 'b+',
        )
    #plot(ixy[:,0], ixy[:,1], 'rs',
    #     fxy[:,0], fxy[:,1], 'b+')

    R = sqrt((ix - cx)**2 + (iy - cy)**2)
    IR = argsort(R)

    mR = []
    mD = []
    mI = []

    #for i in IR:
    for ii in range(len(IR)):
        i = IR[ii]
        r = 5
        x = ix[i]
        y = iy[i]
        #I = find( (fabs(fx - x) < r) * (fabs(fy - y) < r) )
        D = sqrt((fx - x)**2 + (fy - y)**2)
        I = find( D < r )
        for j in I:
            mR.append(R[i])
            mD.append(D[j])
            mI.append(i)

    figure(2)
    clf()
    plot(mR, mD, 'ro')
    xlabel('Distance from quad center')
    ylabel('Match distance')

    figure(3)
    clf()
    plot(mI, mD, 'ro')
    xlabel('Index point number')
    ylabel('Match distance')
