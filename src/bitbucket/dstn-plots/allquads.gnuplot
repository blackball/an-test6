
f(x) = a1*x + b1
fit f(x) "sdss2.quads" using (log($1)):(log($2)) via a1,b1;

g(x) = a*x + b
fit g(x) "galex2.quads" using (log($1)):(log($2)) via a,b;

#g(x) = 6*x + c
#fit g(x) "galex.quads" using (log($1)):(log($2)) via c;

set xlabel "Scale of quads (radius on unit sphere)"
set ylabel "Number of quads"
set key top left
set logscale xy
#set xrange [0.009:0.11]

set terminal postscript
set output "allquads.ps"

plot [x=*: 0.0035] [1 : 1e9 ] "galex.quads" using ($1):($2) with linespoints title "Galex", \
     "sdss.quads"  using ($1):($2) with linespoints title "SDSS", \
     exp(f(log(x))) title "SDSS linear fit (above 1e-4), slope ~ 5.3", \
     exp(g(log(x))) title "Galex linear fit (above 1e-3), slope ~ 5.5"


