
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

#suff = '_0xf9ff_0.dat'
suff = '_0xf9ff_1.dat'
imdat = load('scatter_image'+suff)
redat = load('scatter_ref'+suff)
dedat = load('corr_delta'+suff)

figure()
hold(True)

# Toggle t=2 and t=0 for plotting in RA/Dec or pixel coords
t=2
#t=0
plot(imdat[:,t], imdat[:,t+1], 'r.')
ax = axis()
plot(redat[:,t], redat[:,t+1], 'bs', markerfacecolor=None)

# Plot correspondences; only applys to pixel space
#plot(dedat[:,0], dedat[:,1], 'gd', markeredgecolor='g', markerfacecolor=None, markersize=20)
#for x,y,dx,dy in dedat:
#    line([x,y],[x+dx,y+dy])

#axis(ax)
#axis('equal')
#axis(ax)
show()
#quiver(dedat[:,0], dedat[:,1],dedat[:,2],dedat[:,3],scale=1.0)
savefig('/home/keir/tweak_fit.ps')
title(suff)
show()

figure()
title(suff)
subplot(211)
hist(dedat[:,2],20)
title('dx distribution (pixels)')
subplot(212)
hist(dedat[:,3],20)
title('dy distribution (pixels)')
savefig('/home/keir/tweak_dpx_errs.png')
show()
