from pylab import *
import pyfits

def indexsort(lst):
    def mycmp(i1, i2):
        if lst[i1] < lst[i2]:
            return -1
        if lst[i1] > lst[i2]:
            return 1
        return 0
    inds = range(len(lst))
    inds.sort(cmp=mycmp)
    return inds

hdus = pyfits.open('field1.xy.fits')
table = hdus[1].data
fx = table.field(0)
fy = table.field(1)

hdus = pyfits.open('index1.xy.fits')
table = hdus[1].data
ix = table.field(0)
iy = table.field(1)

hdus=pyfits.open('wcs1.fits')
hdr = hdus[0].header
cx = hdr['CRPIX1']
cy = hdr['CRPIX2']

# arcsec per pix
pixscale = 97.7

#plot(ix, iy, 'g.', fx, fy, 'ro')
#plot(fx, fy, 'r+', ix, iy, 'gx')

xmax = max(fx)
ymax = max(fy)
xmin = 0
ymin = 0

ok = (ix >= xmin) * (ix <= xmax) * (iy >= ymin) * (iy <= ymax);

ix2 = array(list(ix[i] for i in find(ok)))
iy2 = array(list(iy[i] for i in find(ok)))
ix = ix2
iy = iy2

#plot(fx, fy, 'r+', ix, iy, 'gx', array([cx]), array([cy]), 'bs')

sigma = hypot(1 / pixscale, 1)
cutoff = sigma * 6

corri = []
corrf = []
corrd = []
for x,y,fi in zip(fx,fy,range(len(fx))):
    ok = (abs(ix - x) < cutoff) * (abs(iy - y) < cutoff)
    d2 = ((ix - x)**2 + (iy - y)**2)
    ok *= (d2 < (cutoff**2))
    okind = list(find(ok))
    corri += okind
    corrf += ([fi] * len(okind))
    corrd += [sqrt(d2[i]) for i in okind]

cfx = [fx[i] for i in corrf]
cfy = [fy[i] for i in corrf]
cix = [ix[i] for i in corri]
ciy = [iy[i] for i in corri]

corrdc = [sqrt((x - cx)**2 + (y - cy)**2) for x,y in zip(cfx,cfy)]

S = indexsort(corrdc)

figure(1)
plot(cfx, cfy, 'r+', cix, ciy, 'gx', array([cx]), array([cy]), 'bs')
figure(2)
plot([corrdc[i] for i in S], [corrd[i] for i in S], 'r.')

