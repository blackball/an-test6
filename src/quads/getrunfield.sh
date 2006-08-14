#! /bin/bash

tmpfits=`mktemp /tmp/tmp.getrunfield.XXXXXX`

if [ $# -eq 2 ]; then
  start=$1;
  end=$2;
else
  start=1;
  end=35;
fi

echo "$start to $end"
# "tablist" prints the columns in the order they appear in the FITS file,
# NOT the order you ask for them...
echo "FILE FIELDNUM RUN RERUN CAMCOL FIELD FILTER IFIELD"
for ((f=$start; f<=$end; f++)); do
	in=`printf ~/stars/SDSS_FIELDS/sdssfield%02i.fits $f`
	for ((x=1; x<10000; x++)); do
		fitsgetext -i $in -o $tmpfits -e 0 -e $x >/dev/null 2>&1   ||  continue;
		tablist $tmpfits"[#row==1][col RUN;FIELD;RERUN;CAMCOL;FILTER;IFIELD]" | grep -v "\(RUN\|^$\)" | \
			gawk '{printf("%i %i %i %i %i %i %i %i\n", '$f', '$(($x-1))', $2, $3, $4, $5, $6, $7)}'
	done
done
