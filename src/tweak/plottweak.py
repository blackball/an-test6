
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

imdat = load('scatter_image_0xf9ff_13.dat')
redat = load('scatter_ref_0xf9ff_13.dat')
dedat = load('corr_delta_0xf9ff_13.dat')

hold(True)
plot(imdat[:,0], imdat[:,1], 'r.')
plot(redat[:,0], redat[:,1], 'bs', markerfacecolor=None)
plot(dedat[:,0], dedat[:,1], 'gd', markeredgecolor='g', markerfacecolor=None, markersize=20)
for x,y,dx,dy in dedat:
    line([x,y],[x+dx,y+dy])
show()
#quiver(dedat[:,0], dedat[:,1],dedat[:,2],dedat[:,3],scale=1.0)
