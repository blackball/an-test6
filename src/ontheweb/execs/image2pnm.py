#!/usr/bin/env python
"""
Convert an image in a variety of formats into a pnm file

Author: Keir Mierle 2007
"""
import sys
import os

AN_FITSTOPNM = 'anfits2pnm'

imgcmds = {"FITS image data"  : ("fits", AN_FITSTOPNM + " -i %s > %s"),
           "JPEG image data"  : ("jpg",  "jpegtopnm %s > %s"),
           "PNG image data"   : ("png",  "pngtopnm %s > %s"),
           "GIF image data"   : ("gif",  "giftopnm %s > %s"),
           "Netpbm PPM"       : ("pnm",  "ppmtoppm < %s > %s"),
           "Netpbm PGM"       : ("pnm",  "pgmtoppm %s > %s"),
           }

compcmds = {"gzip compressed data"    : ("gz",  "gunzip -c %s > %s"),
            "bzip2 compressed data"   : ("bz2", "bunzip2 -k -c %s > %s")
           }

def do_command(cmd):
    if os.system(cmd) != 0:
        print >>sys.stderr, 'Command failed: %s' % cmd
        return -1

def convert_image(input, output):
    # FIXME does not fits2fits
    filein, fileout = os.popen2('file -b -N -L %s' % input)
    typeinfo = fileout.read()

    print 'OLD:',typeinfo
    
    # Trim extra data after the ,
    comma_pos = typeinfo.find(',')
    if comma_pos != -1:
        typeinfo = typeinfo[:comma_pos]

    print 'NEW:',typeinfo

    # Recurse if we're compressed
    if typeinfo in compcmds:
        new_input = output + '.raw'
        print 'Compressed file, dumping to:',new_input
        do_command(compcmds[typeinfo][1] % (input, new_input))
        return convert_image(new_input, output)

    if not typeinfo in imgcmds:
        print 'ERROR: image type not recognized:', typeinfo
        return -1

    # Do the actual conversion
    output_pnmfile = output + '.pnm'
    do_command(imgcmds[typeinfo][1] % (input, output_pnmfile))

def help():
    print (
        "convert most image types (including gzip'd compressed images) to pnm\n"
        "usage: %s <input-file> <output-prefix>\n"
        "outputs <output-prefix>.inext and <out-prefix>.pnm\n"
        "if the file is compressed, a output.raw file containing\n"
        "the uncompressed file is also produced." % sys.argv[0])

def main():
    if len(sys.argv) != 3:
        print "ERROR incorrect usage"
        help()
        return 1

    return convert_image(sys.argv[1], sys.argv[2])

if __name__ == '__main__':
    sys.exit(main())
