import os
import os.path
import re

import an.gmaps_config as gmaps_config
import quads.image2pnm as image2pnm
import quads.fits2fits as fits2fits

from an.portal.log import log
from an.portal.run_command import run_command

class FileConversionError(Exception):
    errstr = None
    def __init__(self, errstr):
        #super(FileConversionError, self).__init__()
        self.errstr = errstr
    def __str__(self):
        return self.errstr
    
def convert(job, fn, store_imgtype=False, store_imgsize=False):
    log('convert(%s)' % fn)
    tempdir = gmaps_config.tempdir
    basename = os.path.join(tempdir, job.jobid + '-')
    fullfn = basename + fn
    if os.path.exists(fullfn):
        return fullfn

    if fn == 'uncomp':
        orig = job.get_orig_file()
        comp = image2pnm.uncompress_file(orig, fullfn)
        if comp:
            log('Input file compression: %s' % comp)
        if comp is None:
            return orig
        return fullfn

    elif fn == 'pnm':
        infn = convert(job, 'uncomp', store_imgtype, store_imgsize)
        #andir = gmaps_config.basedir + 'quads/'
        log('Converting %s to %s...\n' % (infn, fullfn))
        (imgtype, errstr) = image2pnm.image2pnm(infn, fullfn, None, False, False, None, False)
        if errstr:
            err = 'Error converting image file: %s' % errstr
            log(err)
            raise FileConversionError(errstr)
        if store_imgtype:
            job.imgtype = imgtype
        return fullfn

    elif fn == 'pgm':
        # run 'pnmfile' on the pnm.
        infn = convert(job, 'pnm', store_imgtype, store_imgsize)
        cmd = 'pnmfile %s' % infn
        (filein, fileout) = os.popen2(cmd)
        filein.close()
        out = fileout.read().strip()
        log('pnmfile output: ' + out)
        pat = re.compile(r'P(?P<pnmtype>[BGP])M .*, (?P<width>\d*) by (?P<height>\d*) *maxval \d*')
        match = pat.search(out)
        if not match:
            log('No match.')
            return HttpResponse('couldn\'t find file size')
        w = int(match.group('width'))
        h = int(match.group('height'))
        pnmtype = match.group('pnmtype')
        log('Type %s, w %i, h %i' % (pnmtype, w, h))
        if store_imgsize:
            job.imagew = w
            job.imageh = h
        if pnmtype == 'G':
            return infn
        cmd = 'ppmtopgm %s > %s' % (infn, fullfn)
        log('running: ' + cmd)
        os.system(cmd)
        return fullfn

    elif fn == 'fitsimg':
        if store_imgtype:
            # check the uncompressed input image type...
            infn = convert(job, 'uncomp', store_imgtype, store_imgsize)
            (job.imgtype, errmsg) = image2pnm.get_image_type(infn)
            if errmsg:
                log(errmsg)
                raise FileConversionError(errmsg)

        # fits image: fits2fits it.
        if job.imgtype == image2pnm.fitstype:
            errmsg = fits2fits.fits2fits(infn, fullfn, False)
            if errmsg:
                log(errmsg)
                raise FileConversionError(errmsg)
            return fullfn

        # else, convert to pgm and run pnm2fits.
        infn = convert(job, 'pgm', store_imgtype, store_imgsize)
        cmd = 'pnmtofits %s > %s' % (infn, fullfn)
        log('Running: ' + cmd)
        (rtnval, stdout, stderr) = run_command(cmd)
        if rtnval:
            errmsg = 'pnmtofits failed: ' + stderr
            log(errmsg)
            raise FileConversionError(errmsg)
        return fullfn

    elif fn == 'xyls':
        infn = convert(job, 'fitsimg', store_imgtype, store_imgsize)
        sxylog = job.get_filename('simplexy.out')
        sxyerr = job.get_filename('simplexy.err')
        cmd = 'image2xy -o %s %s > %s 2> %s' % (fullfn, infn, sxylog, sxyerr)
        log('running: ' + cmd)
        (rtn, out, err) = run_command(cmd)
        if rtn:
            errmsg = 'Source extraction failed.'
            log('Image2xy failed: rtn val %d' % rtn)
            raise FileConversionError(errmsg)
        return fullfn

    elif fn == 'wcsinfo':
        infn = job.get_filename('wcs.fits')
        cmd = 'wcsinfo %s > %s' % (infn, fullfn)
        (rtn, stdout, stderr) = run_command(cmd)
        if rtn:
            errmsg = 'wcsinfo failed.'
            log('wcsinfo failed: rtn val %d' % rtn)
            raise FileConversionError(errmsg)
        return fullfn
    
    errmsg = 'Unimplemented: convert(%s)' % fn
    log(errmsg)
    raise FileConversionError(errmsg)

