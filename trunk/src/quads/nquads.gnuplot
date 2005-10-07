#  To generate the data file nquads, do something like the following (in bash):
#
# for x in 0.01 0.02 0.03 0.04 0.05 0.06 0.07 0.08 0.09 0.1; do
#   for r in 1 2 3; do
#     echo $x;
#     echo -n "$x " >> nquads;"
#     ./all_quads $$x | tail -1 >> nquads;"
#     sleep 1;"
#   done;"
# done"
#
# (the sleep seems to be necessary because Andrew Moore's random-seeding code
#  seems to use the system clock, with second resolution).
#

f(x) = a*x + b
fit f(x) "nquads" using (log($1)):(log($2)) via a,b;

g(x) = 6*x + c
fit g(x) "nquads" using (log($1)):(log($2)) via c;

set xlabel "Scale of quads"
set ylabel "Number of quads"
set key top left
set logscale xy
set xrange [0.009:0.11]

set terminal postscript
set output "nquads.ps"

plot "nquads" using ($1):($2) title "Data", exp(f(log(x))) title "Best linear fit (slope~=5.8)", exp(g(log(x))) title "Linear fit, slope=6"
