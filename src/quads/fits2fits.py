import pyfits
import os
import sys

if __name__ == '__main__':
	if len(sys.argv) != 3:
		print "Usage: fits2fits.py input.fits output.fits"
		sys.exit()

	infile = sys.argv[1];
	outfile = sys.argv[2];

	fitsin = pyfits.open(infile)
	fitsin.info()

	fitsin.verify('fix')

	fitsout = pyfits.HDUList()
	hdr = pyfits.PrimaryHDU()
	hdr.header = fitsin[0].header;
	#fitsout.header = fitsin.header;
	hdr.verify('fix')
	fitsout.append(hdr)
	
	for i, hdu in enumerate(fitsin):
		if (i == 0 and hdu.data != None) or isinstance(hdu, pyfits.ImageHDU):
			print hdu.data.shape+(i,)
			if (i == 0):
				print 'Image: Primary HDU (number 0) %sx%s' % hdu.data.shape
			else:
				print 'Image: Extension HDU (number %s) %sx%s' % tuple((i,)+hdu.data.shape)

			hdu.verify('fix')

			fitsout.append(hdu)
			break


	fitsout.info()

	try:
		fitsout.writeto(outfile)
	except IOError:
		# File probably exists
		print 'File %s appears to already exist; deleting!' % outfile
		os.unlink(outfile)
		fitsout.writeto(outfile)
