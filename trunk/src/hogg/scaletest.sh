#!/bin/bash

# make tree for USNO-B1.0 stars
../quads/startree -f allstarsweep ;

# find lots of quads, saving std and err out
numquads=100 ;
scale=1.00 ;
\ln -s allstarsweep.objs allstarsweep.$scale.objs ;
\ln -s allstarsweep.skdt allstarsweep.$scale.skdt ;
../quads/getquads -f allstarsweep.$scale -s $scale -q $numquads ; # > getquads.$scale.log 2> getquads.$scale.errlog ;
../quads/quadidx -f allstarsweep.$scale ;
