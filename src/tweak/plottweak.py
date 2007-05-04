from scipy import *
from pylab import *

def line(v1,v2, color='b', style=None, linewidth=None, alpha=None, zorder=None):
    "Display a line on the current figure"
    l = Line2D(array([v1[0],v2[0]]), array([v1[1],v2[1]]), color=color)
    if style:
        l.set_linestyle(style) # -, --, -., :
    if linewidth:
        l.set_linewidth(linewidth)
    if alpha:
        l.set_alpha(alpha)
    if zorder:
        l.set_zorder(zorder)
    gca().add_line(l)

def plot_ascii_dump(suff, t=0, tle=''):
    "t=0 - pixels: t=2 - ra/dec"
    imdat = load('scatter_image'+suff)
    redat = load('scatter_ref'+suff)
    dedat = load('corr_delta'+suff)

    figure()
    hold(True)

    # Toggle t=2 and t=0 for plotting in RA/Dec or pixel coords
#    if t==2:
#        imdat[imdat[:,0]>10,] -= 
#        mdiff = amax(numpy.subtract.outer(imdat,imdat).ravel())
#        if mdiff > 180:

    plot(imdat[:,t], imdat[:,t+1], 'r.', label="image")
    ax = axis()
    plot(redat[:,t], redat[:,t+1], 'bs', markerfacecolor=None, label="catalog")
    legend()

    if t==0:
        title(tle + 'in image xy space')
    else:
        title(tle + 'in sky ra/dec space')

    if t==0:
        plot(dedat[:,0],dedat[:,1],'b.')

    # Plot correspondences; only applys to pixel space
    #plot(dedat[:,0], dedat[:,1], 'gd', markeredgecolor='g', markerfacecolor=None, markersize=20)
    for x,y,dx,dy,incl in dedat:
        if incl > 0.5:
            line([x,y],[x+dx,y+dy], color='green', linewidth=2)
            plot([x],[y],'gx')
        else:
            line([x,y],[x+dx,y+dy])



    #axis(ax)
    #axis('equal')
    #axis(ax)
    show()
    #quiver(dedat[:,0], dedat[:,1],dedat[:,2],dedat[:,3],scale=1.0)
    savefig('tweak_fit.ps')
    show()

    # Plot error distribution in pixels
    figure()
    subplot(211)
    title(suff)
    n = dedat.shape[0]
    hist(dedat[:,2],2*sqrt(n))
    title('dx distribution (pixels) for %s' % suff)

    subplot(212)
    hist(dedat[:,3],2*sqrt(n))
    title('dy distribution (pixels) for %s' % suff)
    savefig('tweak_dpx_errs.png')
    show()

    figure()
    rayleigh = sqrt(sum(dedat[:,2:4]**2,1))
    hist(rayleigh,2*sqrt(n))
    title('||error|| (pixels) for %s' % suff)
    savefig('tweak_rayleigh_errs.png')
    show()

import glob
plot_ascii_dump("_000.dat",0, "untweaked")
plot_ascii_dump("_001.dat",0, "tweaked")
for i in range(2,400):
    try:
        plot_ascii_dump("_%03i.dat" % i,0, "")
    except IOError:
        break
#plot_ascii_dump("_1.dat",0,"tweaked")
#plot_ascii_dump("_0.dat",2,"untweaked")
#plot_ascii_dump("_1.dat",2,"tweaked")
