#! /usr/bin/env python
import pyfits
import os
import sys
import re

if __name__ == '__main__':
	if len(sys.argv) != 3:
		print "Usage: fits2fits.py input.fits output.fits"
		sys.exit()

	infile = sys.argv[1];
	outfile = sys.argv[2];

	# Read input file.
	fitsin = pyfits.open(infile)
	# Print out info about input file.
	fitsin.info()

	# Create the output HDU
	outhdu = pyfits.PrimaryHDU()

	# verify() fails when a keywords contains invalid characters,
	# so go through the primary header and fix them by converting invalid
	# characters to '_'
	hdr = fitsin[0].header
	cards = hdr.ascardlist()
	# allowed charactors (FITS standard section 5.1.2.1)
	pat = re.compile(r'[^A-Z0-9_\-]')
	for c in cards.keys():
		#print c
		# new keyword:
		cnew = pat.sub('_', c)
		if (c != cnew):
			print "Replacing illegal keyword ", c, " by ", cnew
			# add the new header card
			hdr.update(cnew, cards[c].value, cards[c].comment, after=c)
			# remove the old one.
			del hdr[c]

	# Fix input primary header
	fitsin[0].verify('fix');
	# Copy fixed input header to output
	outhdu.header = fitsin[0].header;
	
	for i, hdu in enumerate(fitsin):
		if (i == 0 and hdu.data != None) or isinstance(hdu, pyfits.ImageHDU):
			if (i == 0):
				print 'Image: Primary HDU (number 0) %sx%s' % hdu.data.shape
			else:
				print 'Image: Extension HDU (number %s) %sx%s' % tuple((i,)+hdu.data.shape)

			# Copy fixed HDU data to output.
			hdu.verify('fix');
			outhdu.data = hdu.data;
			break

	# Create output file
	fitsout = pyfits.HDUList()
	fitsout.append(outhdu)
	# Describe output file we're about to write...
	fitsout.info()

	try:
		fitsout.writeto(outfile)
	except IOError:
		# File probably exists
		print 'File %s appears to already exist; deleting!' % outfile
		os.unlink(outfile)
		fitsout.writeto(outfile)
	except VerifyError:
		print 'Verification of output file failed: your FITS file is probably too broken to automatically fix.';
		sys.exit(1)
