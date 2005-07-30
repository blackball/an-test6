#!/bin/bash

# make tree for USNO-B1.0 stars
../quads/startree -f allstarsweep > startree.log 2> startree.errlog ;

# find lots of quads, saving std and err out
\rm -rvf *?.[01].[013][013].?*
numquads=10000000 ;
for scale in 0.01 0.03 0.10 0.30 1.00
  do
  echo $scale ;
  \ln -s allstarsweep.objs allstarsweep.$scale.objs ;
  \ln -s allstarsweep.skdt allstarsweep.$scale.skdt ;
  echo running getquads ;
  ../quads/getquads -f allstarsweep.$scale -s $scale -q $numquads > getquads.$scale.log 2> getquads.$scale.errlog ;
  echo running quadidx ;
  ../quads/quadidx -f allstarsweep.$scale > quadidx.$scale.log 2> quadidx.$scale.errlog ;
done

