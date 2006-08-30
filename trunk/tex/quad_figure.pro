;+
; BUGS:
;   - No proper code header.
;   - Should use hogg_usersym.
;   - Should use djs_icolor.
;   - Should use hogg_plot_defaults.
;   - "small" quantity hacked.
;   - Not good for overlapping symbols.
;   - "length" assignment needs to be checked.
;-
function quad_random_point, seed
rr2= 0.5*randomu(seed)
theta= 2.0*!DPI*randomu(seed)
return, ([0.5,0.5]+sqrt(rr2)*[cos(theta),sin(theta)])
end

pro quad_figure, seed
if (not keyword_set(seed)) then seed=3L

prefix='quad'
xsize= 7.5 & ysize= 7.5
set_plot, "PS"
device, file=prefix+'.ps',/inches,xsize=xsize,ysize=ysize, $
  xoffset=(8.5-xsize)/2.0,yoffset=(11.0-ysize)/2.0
; hogg_plot_defaults
!X.OMARGIN=[0,0]
!X.MARGIN=[0,0]
!Y.OMARGIN=0.6*!X.OMARGIN
!Y.MARGIN=0.6*!X.MARGIN
!X.RANGE=[-0.5,0.5]*1.1*sqrt(2.0)
!Y.RANGE=!X.RANGE
!X.STYLE= 5
!Y.STYLE= 5
xyouts, 0,0,'!3'

nx= 4
!P.MULTI=[0,nx,nx]
!P.CHARTHICK= 3.0/nx
charsize= 2.0/nx
gridthick= 1.0/nx
codethick= 4.0/nx
labelboxsize= 8.0/nx
starsize= 2.0/nx
stcharthick= 1.0*codethick
factor= 0.5
stcharsize= factor*charsize
gridcolor= 127
tt= 2D0*!DPI*dindgen(2001)/2D3
for ii=1,nx do for jj=1,nx do begin

    theta= 2.0*!DPI*randomu(seed)
    length= randomu(seed)^(1.0/6.0)  ; guess
    mm= [[cos(theta),sin(theta)],[-sin(theta),cos(theta)]]
    aaa= [-0.5,-0.5]*length
    bbb= [0.5,0.5]*length
    left= [0.5,-0.5]*length
    up= [-0.5,0.5]*length
    ccc= quad_random_point(seed)*length+aaa
    cright= [ccc[0],aaa[1]]
    cdown= [aaa[0],ccc[1]]
    ddd= quad_random_point(seed)*length+aaa
    dright= [ddd[0],aaa[1]]
    ddown= [aaa[0],ddd[1]]

; plot square
    plot, [(mm#aaa)[0],(mm#left)[0],(mm#bbb)[0],(mm#up)[0],(mm#aaa)[0]], $
      [(mm#aaa)[1],(mm#left)[1],(mm#bbb)[1],(mm#up)[1],(mm#aaa)[1]], $
      thick=gridthick,color=gridcolor
    small= 0.013*(!Y.CRANGE[1]-!Y.CRANGE[0]) ; HACK!
    stsmall= factor*small

; plot circle
    xx= 0.5*(aaa[0]+bbb[0])+length/sqrt(2.0)*cos(tt)
    yy= 0.5*(aaa[0]+bbb[0])+length/sqrt(2.0)*sin(tt)
    oplot, xx,yy,thick=gridthick,color=gridcolor

; plot code lengths
    oplot, [(mm#cright)[0],(mm#ccc)[0]], $
      [(mm#cright)[1],(mm#ccc)[1]], $
      thick=codethick,color=gridcolor
    oplot, [(mm#cright)[0],(mm#aaa)[0]], $
      [(mm#cright)[1],(mm#aaa)[1]], $
      thick=gridthick,color=gridcolor
    oplot, [(mm#cdown)[0],(mm#ccc)[0]], $
      [(mm#cdown)[1],(mm#ccc)[1]], $
      thick=codethick,color=gridcolor
    oplot, [(mm#cdown)[0],(mm#aaa)[0]], $
      [(mm#cdown)[1],(mm#aaa)[1]], $
      thick=gridthick,color=gridcolor
    oplot, [(mm#dright)[0],(mm#ddd)[0]], $
      [(mm#dright)[1],(mm#ddd)[1]], $
      thick=codethick,color=gridcolor
    oplot, [(mm#dright)[0],(mm#aaa)[0]], $
      [(mm#dright)[1],(mm#aaa)[1]], $
      thick=gridthick,color=gridcolor
    oplot, [(mm#ddown)[0],(mm#ddd)[0]], $
      [(mm#ddown)[1],(mm#ddd)[1]], $
      thick=codethick,color=gridcolor
    oplot, [(mm#ddown)[0],(mm#aaa)[0]], $
      [(mm#ddown)[1],(mm#aaa)[1]], $
      thick=gridthick,color=gridcolor

    Cu= 'C!d!8u!3!n'
    Cv= 'C!d!8v!3!n'
    Du= 'D!d!8u!3!n'
    Dv= 'D!d!8v!3!n'
    xl= (mm#(0.5*(cdown+ccc)))[0]
    yl= (mm#(0.5*(cdown+ccc)))[1]
    xyouts, xl,yl-small, $
      Cu,charsize=charsize,align=0.5
    xl= (mm#(0.5*(cright+ccc)))[0]
    yl= (mm#(0.5*(cright+ccc)))[1]
    xyouts, xl,yl-small, $
      Cv,charsize=charsize,align=0.5
    xl= (mm#(0.5*(ddown+ddd)))[0]
    yl= (mm#(0.5*(ddown+ddd)))[1]
    xyouts, xl,yl-small, $
      Du,charsize=charsize,align=0.5
    xl= (mm#(0.5*(dright+ddd)))[0]
    yl= (mm#(0.5*(dright+ddd)))[1]
    xyouts, xl,yl-small, $
      Dv,charsize=charsize,align=0.5

    xl= !X.CRANGE[0]+0.50*(!X.CRANGE[1]-!X.CRANGE[0])
    yl= !Y.CRANGE[0]+0.02*(!Y.CRANGE[1]-!Y.CRANGE[0])
    format= '(F5.3)'
    signstr= ((ccc[0]-aaa[0]) > 0) ? '+' : '-'
    custr= signstr+string(abs(ccc[0]-aaa[0])/length,format=format)
    signstr= ((ccc[1]-aaa[1]) > 0) ? '+' : '-'
    cvstr= signstr+string(abs(ccc[1]-aaa[1])/length,format=format)
    signstr= ((ddd[0]-aaa[0]) > 0) ? '+' : '-'
    dustr= signstr+string(abs(ddd[0]-aaa[0])/length,format=format)
    signstr= ((ddd[1]-aaa[1]) > 0) ? '+' : '-'
    dvstr= signstr+string(abs(ddd[1]-aaa[1])/length,format=format)
    xyouts, xl,yl,'('+Cu+','+Cv+','+Du+','+Dv+') = ' $
      +'('+custr+','+cvstr+','+dustr+','+dvstr+')', $
      charsize=0.75*charsize,align=0.5

    usersym, [1,-1,-1,1,1],[1,1,-1,-1,1],/fill
    oplot, [(mm#aaa)[0],(mm#bbb)[0],(mm#ccc)[0],(mm#ddd)[0]], $
      [(mm#aaa)[1],(mm#bbb)[1],(mm#ccc)[1],(mm#ddd)[1]], $
      psym=8,symsize=starsize
    xyouts, (mm#aaa)[0],(mm#aaa)[1]-stsmall,'A',charsize=stcharsize, $
      color=255,align=0.5,charthick=stcharthick
    xyouts, (mm#bbb)[0],(mm#bbb)[1]-stsmall,'B',charsize=stcharsize, $
      color=255,align=0.5,charthick=stcharthick
    xyouts, (mm#ccc)[0],(mm#ccc)[1]-stsmall,'C',charsize=stcharsize, $
      color=255,align=0.5,charthick=stcharthick
    xyouts, (mm#ddd)[0],(mm#ddd)[1]-stsmall,'D',charsize=stcharsize, $
      color=255,align=0.5,charthick=stcharthick

endfor
device,/close
return
end
