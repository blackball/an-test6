#!/bin/bash

# make tree for USNO-B1.0 stars
echo running startree... ;
../quads/startree -f allstarsweep > startree.log 2> startree.errlog ;
echo ...done ;

# find lots of quads, saving std and err out
\rm -rvf *?.0.000[013][013].?*
numquads=10000000 ;
for scale in 0.00001 0.00003 0.00010 0.00030
  do
  echo scale = $scale ;
  \ln -s allstarsweep.objs allstarsweep.$scale.objs ;
  \ln -s allstarsweep.skdt allstarsweep.$scale.skdt ;
  echo running getquads... ;
  ../quads/getquads -f allstarsweep.$scale -s $scale -q $numquads > getquads.$scale.log 2> getquads.$scale.errlog ;
  echo ...done ;
  echo running quadidx... ;
  ../quads/quadidx -f allstarsweep.$scale > quadidx.$scale.log 2> quadidx.$scale.errlog ;
  echo ...done ;
done
