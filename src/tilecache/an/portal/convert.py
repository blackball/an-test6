import math
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

def run_convert_command(cmd):
    log('Command: ' + cmd)
    (rtn, stdout, stderr) = run_command(cmd)
    if rtn:
        errmsg = 'Command failed: ' + cmd + ': ' + stderr
        log(errmsg + '; rtn val %d' % rtn)
        log('out: ' + stdout);
        log('err: ' + stderr);
        raise FileConversionError(errmsg)

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
        run_convert_command(cmd)
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
        run_convert_command(cmd)
        return fullfn

    elif fn == 'xyls':
        infn = convert(job, 'fitsimg', store_imgtype, store_imgsize)
        sxylog = job.get_filename('simplexy.out')
        sxyerr = job.get_filename('simplexy.err')
        cmd = 'image2xy -o %s %s > %s 2> %s' % (fullfn, infn, sxylog, sxyerr)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'wcsinfo':
        infn = job.get_filename('wcs.fits')
        cmd = 'wcsinfo %s > %s' % (infn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'objsinfield':
        infn = job.get_filename('wcs.fits')
        cmd = 'plot-constellations -L -w %s -N -C -B -b 10 -j > %s' % (infn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'fullsizepng':
        fullfn = job.get_filename('fullsize.png')
        if os.path.exists(fullfn):
            return fullfn
        infn = convert(job, 'pnm', store_imgtype, store_imgsize)
        cmd = 'pnmtopng %s > %s' % (infn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'indexxyls':
        wcsfn = job.get_filename('wcs.fits')
        indexrdfn = job.get_filename('index.rd.fits')
        if not (os.path.exists(wcsfn) and os.path.exists(indexrdfn)):
            errmsg('indexxyls: WCS and Index rdls files don\'t exist.')
            raise FileConversionError(errmsg)
        cmd = 'wcs-rd2xy -w %s -i %s -o %s' % (wcsfn, indexrdfn, fullfn)
        run_convert_command(cmd)
        return fullfn

    #elif fn == 'overlay':
    #    imgfn = convert(job, 'pnm-small-dim', store_imgtype, store_imgsize)
    #    ixyfn = convert(job, 'indexxyls', store_imgtype, store_imgsize)

    elif fn == 'pnm-small':
        imgfn = convert(job, 'pnm', store_imgtype, store_imgsize)
        if not job.displayscale:
            w = job.imagew
            h = job.imageh
            job.displayscale = max(1.0, math.pow(2, math.ceil(math.log(max(w, h) / float(800)) / math.log(2))))
            job.displayw = int(round(w / float(job.displayscale)))
            job.displayh = int(round(h / float(job.displayscale)))
            job.save()
        # (don't make this an elif)
        if job.displayscale == 1:
            return imgfn
        cmd = 'pnmscale -reduce %g %s > %s' % (float(job.displayscale), imgfn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'annotation':
        imgfn = convert(job, 'pnm-small', store_imgtype, store_imgsize)
        wcsfn = job.get_filename('wcs.fits')
        cmd = ('plot-constellations -N -w %s -o %s -C -B -b 10 -j -s %g -i %s' %
               (wcsfn, fullfn, 1.0/job.displayscale, imgfn))
        run_convert_command(cmd)
        return fullfn

    elif fn == 'annotation-big':
        imgfn = convert(job, 'pnm', store_imgtype, store_imgsize)
        wcsfn = job.get_filename('wcs.fits')
        cmd = ('plot-constellations -N -w %s -o %s -C -B -b 10 -j -i %s' %
               (wcsfn, fullfn, imgfn))
        run_convert_command(cmd)
        return fullfn

    errmsg = 'Unimplemented: convert(%s)' % fn
    log(errmsg)
    raise FileConversionError(errmsg)

