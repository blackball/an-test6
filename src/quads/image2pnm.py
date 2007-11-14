#!/usr/bin/env python
"""
Convert an image in a variety of formats into a pnm file

Author: Keir Mierle 2007
"""
import sys
import os
import tempfile
from fits2fits import fits2fits as fits2fits

fitstype = "FITS image data"

imgcmds = {fitstype : ("fits", "an-fitstopnm -i %s > %s"),
           "JPEG image data"  : ("jpg",  "jpegtopnm %s > %s"),
           "PNG image data"   : ("png",  "pngtopnm %s > %s"),
           "GIF image data"   : ("gif",  "giftopnm %s > %s"),
           "Netpbm PPM"       : ("pnm",  "ppmtoppm < %s > %s"),
           "Netpbm PPM \"rawbits\" image data" : ("pnm",  "cp %s %s"),
           "Netpbm PGM"       : ("pnm",  "pgmtoppm %s > %s"),
           "Netpbm PGM \"rawbits\" image data" : ("pnm",  "pgmtoppm %s > %s"),
           "TIFF image data"  : ("tiff",  "tifftopnm %s > %s"),
           }

compcmds = {"gzip compressed data"    : ("gz",  "gunzip -c %s > %s"),
            "bzip2 compressed data"   : ("bz2", "bunzip2 -k -c %s > %s")
           }

def shell_escape(s):
    repl = ("\\", "|", "&", ";", "(", ")", "<", ">", " ", "\t", "\n", "'", "\"")
    # (note, "\\" must be first!)
    for x in repl:
        s = s.replace(x, '\\'+x)
    return s

def log(*x):
    print >> sys.stderr, 'image2pnm:', ' '.join(x)

def do_command(cmd):
    if os.system(cmd) != 0:
        print >>sys.stderr, 'Command failed: %s' % cmd
        sys.exit(-1)

# Run the "file" command, return the trimmed output.
def run_file(fn):
    (filein, fileout) = os.popen2('file -b -N -L %s' % shell_escape(fn))
    typeinfo = fileout.read().strip()
    # Trim extra data after the ,
    comma_pos = typeinfo.find(',')
    if comma_pos != -1:
        typeinfo = typeinfo[:comma_pos]
    return typeinfo

def uncompress_file(infile, uncompressed, outdir=None, typeinfo=None, quiet=True):
    """
    infile: input filename.
    uncompressed: output filename.
    typeinfo: output from the 'file' command; if None we'll run 'file'.
    quiet: don't print any informational messages.

    Returns: comptype
    comptype: None if the file wasn't compressed, or 'gz' or 'bz2'.
    """
    if not typeinfo:
        typeinfo = run_file(infile)
    if not typeinfo in compcmds:
        return None
    assert uncompressed != infile
    if not quiet:
        log('compressed file, dumping to:', uncompressed)
    (ext, cmd) = compcmds[typeinfo]
    do_command(cmd % (shell_escape(infile), shell_escape(uncompressed)))
    return ext

def get_image_type(infile):
    typeinfo = run_file(infile)
    if not typeinfo in imgcmds:
        return (None, 'Unknown image type "%s"' % typeinfo)
    (ext, cmd) = imgcmds[typeinfo]
    return (ext, None)

def image2pnm(infile, outfile, sanitized, force_ppm, no_fits2fits,
              mydir, quiet):
    """
    infile: input filename.
    outfile: output filename.
    sanitized: for FITS images, output filename of sanitized (fits2fits'd) image.
    force_ppm: boolean, convert PGM to PPM so that the output is always PPM.

    Returns: (type, error)

    - type: (string): image type: 'jpg', 'png', 'gif', etc., or None if
       image type isn't recognized.

    - error: (string): error string, or None
    """
    typeinfo = run_file(infile)
    if not typeinfo in imgcmds:
        return (None, 'Image type not recognized: "%s"' % typeinfo)

    tempfiles = []

    # If it's a FITS file we want to filter it first because of the many
    # misbehaved FITS files. fits2fits is a sanitizer.
    if (typeinfo == fitstype) and (not no_fits2fits):
        if not sanitized:
            (outfile_dir, outfile_file) = os.path.split(outfile)
            (f, sanitized) = tempfile.mkstemp('sanitized', outfile_file, outfile_dir)
            os.close(f)
            tempfiles.append(sanitized)
        else:
            assert sanitized != infile
        errstr = fits2fits(infile, sanitized, not quiet)
        if errstr:
            return (None, errstr)
        infile = sanitized

    if force_ppm:
        original_outfile = outfile
        (outfile_dir, outfile_file) = os.path.split(outfile)
        (f, outfile) = tempfile.mkstemp('pnm', outfile_file, outfile_dir)
        os.close(f)
        if not quiet:
            log('temporary output file: ', outfile)

    # Do the actual conversion
    ext, cmd = imgcmds[typeinfo]
    # an-fitstopnm: add path...
    if (typeinfo == fitstype) and mydir:
        cmd = mydir + cmd
    if quiet:
        cmd = cmd + " 2>/dev/null"
    do_command(cmd % (shell_escape(infile), shell_escape(outfile)))

    if force_ppm:
        typeinfo = run_file(outfile)
        if (typeinfo.startswith("Netpbm PGM")):
            # Convert to PPM.
            do_command("pgmtoppm white %s > %s" % (shell_escape(outfile), shell_escape(original_outfile)))
            tempfiles.append(outfile)
        else:
            os.rename(outfile, original_outfile)

    for fn in tempfiles:
        os.unlink(fn)

    # Success
    return (ext, None)
    

def convert_image(infile, outfile, uncompressed, sanitized, force_ppm, no_fits2fits,
                  mydir, quiet):
    typeinfo = run_file(infile)

    tempfiles = []
    if not uncompressed:
        (outfile_dir, outfile_file) = os.path.split(outfile)
        (f, uncompressed) = tempfile.mkstemp(None, 'uncomp', outdir)
        os.close(f)
        tempfiles.append(uncompressed)

    comp = uncompress_file(infile, uncompressed, typeinfo, quiet)
    if comp:
        print 'compressed'
        print comp
        infile = uncompressed

    (imgtype, errstr) = image2pnm(infile, outfile, sanitized, force_ppm, no_fits2fits, mydir, quiet)

    for fn in tempfiles:
        os.unlink(fn)

    if errstr:
        log('ERROR: %s' % errstr)
        return -1
    print imgtype
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
    parser.add_option("-2", "--no-fits2fits",
                      action="store_true", dest="no_fits2fits",
                      help="don't sanitize FITS files");
    parser.add_option("-q", "--quiet",
                      action="store_true", dest="quiet",
                      help="only print errors");

    (options, args) = parser.parse_args()

    if not options.infile:
        parser.error('required argument missing: infile')
    if not options.outfile:
        parser.error('required argument missing: outfile')

    #if not options.uncompressed_outfile:
    #    options.uncompressed_outfile = options.infile+'.raw'
    #if not options.sanitized_outfile:
    #    options.sanitized_outfile = options.infile+'.sanitized.fits'

    # Find the path to this executable and use it to find other Astrometry.net
    # executables.
    if (len(sys.argv) > 0):
        mydir = os.path.dirname(sys.argv[0]) + "/"

    #log('input file: ', options.infile);
    #log('output file: ', options.outfile);

    return convert_image(options.infile, options.outfile,
                         options.uncompressed_outfile,
                         options.sanitized_outfile,
                         options.force_ppm,
                         options.no_fits2fits,
                         mydir, options.quiet)

if __name__ == '__main__':
    sys.exit(main())
