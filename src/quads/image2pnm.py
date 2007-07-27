#!/usr/bin/env python
"""
Convert an image in a variety of formats into a pnm file

Author: Keir Mierle 2007
"""
import sys
import os
import tempfile

imgcmds = {"FITS image data"  : ("fits", "an-fitstopnm -i %s > %s"),
           "JPEG image data"  : ("jpg",  "jpegtopnm %s > %s"),
           "PNG image data"   : ("png",  "pngtopnm %s > %s"),
           "GIF image data"   : ("gif",  "giftopnm %s > %s"),
           "Netpbm PPM"       : ("pnm",  "ppmtoppm < %s > %s"),
           "Netpbm PGM"       : ("pnm",  "pgmtoppm %s > %s"),
           }

compcmds = {"gzip compressed data"    : ("gz",  "gunzip -c %s > %s"),
            "bzip2 compressed data"   : ("bz2", "bunzip2 -k -c %s > %s")
           }

def log(*x):
    print >> sys.stderr, 'image2pnm:', ' '.join(x)

def do_command(cmd):
    if os.system(cmd) != 0:
        print >>sys.stderr, 'Command failed: %s' % cmd
        sys.exit(-1)

def convert_image(infile, outfile, uncompressed, sanitized, force_ppm):
    filein, fileout = os.popen2('file -b -N -L %s' % infile)
    typeinfo = fileout.read()
    log('file output:', typeinfo.strip())
    
    # Trim extra data after the ,
    comma_pos = typeinfo.find(',')
    if comma_pos != -1:
        typeinfo = typeinfo[:comma_pos]

    # If it's a FITS file we want to filter it first because of the many
    # misbehaved FITS files around. fits2fits is a sanitizer.
    if typeinfo == 'FITS image data':
        assert sanitized != infile
        new_infile = sanitized
        do_command('fits2fits.py %s %s' % (infile, new_infile))
        infile = new_infile

    # Recurse if we're compressed
    if typeinfo in compcmds:
        assert uncompressed != infile
        new_infile = uncompressed
        log('compressed file, dumping to:', new_infile)
        do_command(compcmds[typeinfo][1] % (infile, new_infile))
        return convert_image(new_infile, outfile, uncompressed, sanitized, force_ppm)

    if not typeinfo in imgcmds:
        log('ERROR: image type not recognized:', typeinfo)
        return -1

    if force_ppm:
        original_outfile = outfile
        outfile_dir, outfile_file = os.path.split(outfile)
        filehandle, outfile = tempfile.mkstemp('pnm', outfile_file, outfile_dir)
        log('temporary output file: ', outfile)

    # Do the actual conversion
    ext, cmd = imgcmds[typeinfo]
    do_command(cmd % (infile, outfile))
    print ext

    if force_ppm:
        filein, fileout = os.popen2('file -b -N -L %s' % outfile)
        typeinfo = fileout.read()
        typeinfo.strip()
        if (typeinfo.startswith("Netpbm PGM")):
            # Convert to PPM.
            do_command("pgmtoppm white %s > %s" % (outfile, original_outfile))
            os.unlink(outfile)
        else:
            os.rename(outfile, original_outfile)

    # Success
    return 0

def main():
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option("-i", "--infile",
                      dest="infile",
                      help="input image FILE", metavar="FILE")
    parser.add_option("-u", "--uncompressed-outfile",
                      dest="uncompressed_outfile",
                      help="uncompressed temporary FILE", metavar="FILE",
                      default='')
    parser.add_option("-s", "--sanitized-fits-outfile",
                      dest="sanitized_outfile",
                      help="sanitized temporary fits FILE", metavar="FILE",
                      default='')
    parser.add_option("-o", "--outfile",
                      dest="outfile",
                      help="output pnm image FILE", metavar="FILE")
    parser.add_option("-p", "--ppm",
                      action="store_true", dest="force_ppm",
                      help="convert the output to PPM");

    (options, args) = parser.parse_args()

    if not options.infile:
        parser.error('required argument missing: infile')
    if not options.outfile:
        parser.error('required argument missing: outfile')

    if not options.uncompressed_outfile:
        options.uncompressed_outfile = options.infile+'.raw'
    if not options.sanitized_outfile:
        options.sanitized_outfile = options.infile+'.sanitized.fits'

    return convert_image(options.infile, options.outfile,
                         options.uncompressed_outfile,
                         options.sanitized_outfile,
                         options.force_ppm)

if __name__ == '__main__':
    sys.exit(main())
