import math
import os
import os.path
import re

import an.gmaps_config as gmaps_config
import quads.image2pnm as image2pnm
import quads.fits2fits as fits2fits

from an.portal.log import log
from an.util.run_command import run_command

class FileConversionError(Exception):
    errstr = None
    def __init__(self, errstr):
        #super(FileConversionError, self).__init__()
        self.errstr = errstr
    def __str__(self):
        return self.errstr

def run_convert_command(cmd, deleteonfail=None):
    log('Command: ' + cmd)
    (rtn, stdout, stderr) = run_command(cmd)
    if rtn:
        errmsg = 'Command failed: ' + cmd + ': ' + stderr
        log(errmsg + '; rtn val %d' % rtn)
        log('out: ' + stdout);
        log('err: ' + stderr);
        if deleteonfail:
            os.unlink(deleteonfail)
        raise FileConversionError(errmsg)

def run_pnmfile(fn):
    cmd = 'pnmfile %s' % fn
    (filein, fileout) = os.popen2(cmd)
    filein.close()
    out = fileout.read().strip()
    log('pnmfile output: ' + out)
    pat = re.compile(r'P(?P<pnmtype>[BGP])M .*, (?P<width>\d*) by (?P<height>\d*) *maxval (?P<maxval>\d*)')
    match = pat.search(out)
    if not match:
        log('No match.')
        return None
    w = int(match.group('width'))
    h = int(match.group('height'))
    pnmtype = match.group('pnmtype')
    maxval = int(match.group('maxval'))
    log('Type %s, w %i, h %i, maxval %i' % (pnmtype, w, h, maxval))
    return (w, h, pnmtype, maxval)

def is_tarball(fn):
    typeinfo = image2pnm.run_file(fn)
    log('file type: "%s"' % typeinfo)
    return typeinfo == 'POSIX tar archive'

def convert(job, df, fn, args=None):
    if args is None:
        args = {}
    log('convert(%s, args=%s)' % (fn, str(args)))
    tempdir = gmaps_config.tempdir
    basename = os.path.join(tempdir, job.get_id() + '-')
    fullfn = basename + fn
    exists = os.path.exists(fullfn)

    variant = 0
    if 'variant' in args:
        variant = int(args['variant'])

    # Hack
    if exists and not variant:
        return fullfn

    if fn == 'uncomp' or fn == 'uncomp-js':
        orig = df.get_path()
        #log('convert uncomp: Field file is', orig)
        comp = image2pnm.uncompress_file(orig, fullfn)
        if comp:
            log('Input file compression: %s' % comp)
        if comp is None:
            return orig
        return fullfn

    elif fn == 'pnm':
        infn = convert(job, df, 'uncomp')
        #andir = gmaps_config.basedir + 'quads/'
        log('Converting %s to %s...\n' % (infn, fullfn))
        (filetype, errstr) = image2pnm.image2pnm(infn, fullfn, None, False, False, None, False)
        if errstr:
            err = 'Error converting image file: %s' % errstr
            log(err)
            raise FileConversionError(errstr)
        df.filetype = filetype
        return fullfn

    elif fn == 'getimagesize':
        infn = convert(job, df, 'pnm')
        x = run_pnmfile(infn)
        if x is None:
            raise FileConversionError('couldn\'t find file size')
        (w, h, pnmtype, maxval) = x
        log('Type %s, w %i, h %i' % (pnmtype, w, h))
        df.imagew = w
        df.imageh = h
        return None

    elif fn == 'pgm':
        # run 'pnmfile' on the pnm.
        infn = convert(job, df, 'pnm')
        x = run_pnmfile(infn)
        if x is None:
            raise FileConversionError('couldn\'t find file size')
        (w, h, pnmtype, maxval) = x
        log('Type %s, w %i, h %i' % (pnmtype, w, h))
        df.imagew = w
        df.imageh = h
        if pnmtype == 'G':
            return infn
        cmd = 'ppmtopgm %s > %s' % (infn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif (fn == 'ppm') or (fn == 'ppm-8bit'):
        eightbit = (fn == 'ppm-8bit')
        if job.is_input_fits() or job.is_input_text():
            # just create the red-circle plot.
            xylist = job.get_filename('job.axy')
            cmd = ('plotxy -i %s -W %i -H %i -x 1 -y 1 -C brightred -w 2 -P > %s' %
                   (xylist, df.imagew, df.imageh, fullfn))
            run_convert_command(cmd)
            return fullfn

        imgfn = convert(job, df, 'pnm')
        x = run_pnmfile(imgfn)
        if x is None:
            raise FileConversionError('pnmfile failed')
        (w, h, pnmtype, maxval) = x
        if pnmtype == 'P':
            if eightbit and (maxval != 255):
                cmd = 'pnmdepth 255 "%s" > "%s"' % (imgfn, fullfn)
                run_convert_command(cmd)
                return fullfn
            return imgfn
        if eightbit and (maxval != 255):
            cmd = 'pnmdepth 255 "%s" | pgmtoppm white > "%s"' % (imgfn, fullfn)
        else:
            cmd = 'pgmtoppm white "%s" > "%s"' % (imgfn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'fitsimg':
        # check the uncompressed input image type...
        infn = convert(job, df, 'uncomp')
        (df.filetype, cmd, errmsg) = image2pnm.get_image_type(infn)
        if errmsg:
            log(errmsg)
            raise FileConversionError(errmsg)

        # fits image: fits2fits it.
        if df.filetype == image2pnm.fitstype:
            errmsg = fits2fits.fits2fits(infn, fullfn, False)
            if errmsg:
                log(errmsg)
                raise FileConversionError(errmsg)
            return fullfn

        # else, convert to pgm and run pnm2fits.
        infn = convert(job, df, 'pgm')
        cmd = 'pnmtofits %s > %s' % (infn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'xyls' or fn == 'xyls-exists?':
        check_exists = (fn == 'xyls-exists?')
        if variant:
            fn = 'xyls-%i' % variant
            fullfn = basename + fn
            if os.path.exists(fullfn):
                return fullfn

        if check_exists:
            if os.path.exists(fullfn):
                return fullfn
            return None

        if job.is_input_text():
            infn = convert(job, df, 'uncomp')
            cmd = 'xylist2fits %s %s' % (infn, fullfn)
            run_convert_command(cmd)
            return fullfn

        if job.is_input_fits():
            # For FITS bintable uploads: put it through fits2fits.
            infn = convert(job, df, 'uncomp')
            errmsg = fits2fits.fits2fits(infn, fullfn, False)
            if errmsg:
                log(errmsg)
                raise FileConversionError(errmsg)
            return fullfn

        sxylog = 'blind.log'

        extraargs = ''
        if variant:
            altargs = {
                1: '',
                2: '',
                3: '',
                4: '',
                }
            if variant in altargs:
                extraargs = altargs[variant]
            # HACK
            sxylog = '/tmp/alternate-xylist-%s.log' % variant

        infn = convert(job, df, 'fitsimg')
        cmd = 'image2xy %s-o %s %s >> %s 2>&1' % (extraargs, fullfn, infn, sxylog)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'xyls-half':
        infn = convert(job, df, 'fitsimg')
        sxylog = 'blind.log'
        cmd = 'image2xy -H -o %s %s >> %s 2>&1' % (fullfn, infn, sxylog)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'xyls-sorted' or fn == 'xyls-half-sorted':
        if fn == 'xyls-sorted':
            infn = convert(job, df, 'xyls')
        else:
            infn = convert(job, df, 'xyls-half')
        logfn = 'blind.log'
        cmd = 'resort-xylist -d %s %s 2>> %s' % (infn, fullfn, logfn)
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
        if job.is_input_fits() or job.is_input_text():
            # just create the red-circle plot.
            xylist = job.get_filename('job.axy')
            cmd = ('plotxy -i %s -W %i -H %i -x 1 -y 1 -C brightred -w 2 > %s' %
                   (xylist, df.imagew, df.imageh, fullfn))
            run_convert_command(cmd)
            return fullfn
        infn = convert(job, df, 'pnm')
        cmd = 'pnmtopng %s > %s' % (infn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'jpeg-norm':
        infn = convert(job, df, 'pnm')
        #cmd = 'pnmtojpeg %s > %s' % (infn, fullfn)
        #cmd = 'pnmnorm %s | pnmtojpeg > %s' % (infn, fullfn)
        cmd = 'pnmnorm -keephues -wpercent 1 %s | pnmtojpeg > %s' % (infn, fullfn)
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

    elif fn == 'thumbnail':
        imgfn = convert(job, df, 'pnm-thumb')
        cmd = 'pnmtopng %s > %s' % (imgfn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif ((fn == 'pnm-small') or
          (fn == 'pnm-medium') or
          (fn == 'pnm-thumb')):
        small = (fn == 'pnm-small')
        thumb = (fn == 'pnm-thumb')
        imgfn = convert(job, df, 'pnm')
        log('in convert(%s): df = %s' % (fn, str(df)))
        if small:
            (scale, w, h) = df.get_small_scale()
        elif thumb:
            (scale, w, h) = df.get_thumbnail_scale()
        else:
            (scale, w, h) = df.get_medium_scale()
        if scale == 1:
            return imgfn
        #cmd = 'pnmscale -reduce %g %s > %s' % (float(scale), imgfn, fullfn)
        cmd = 'pnmscale -width=%g -height=%g %s > %s' % (w, h, imgfn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif ((fn == 'ppm-small') or (fn == 'ppm-small-8bit') or
          (fn == 'ppm-medium') or (fn == 'ppm-medium-8bit')):
        eightbit = (fn == 'ppm-small-8bit') or (fn == 'ppm-medium-8bit')
        small = (fn == 'ppm-small-8bit') or (fn == 'ppm-small')

        # what the heck is this??
        if job.is_input_fits() or job.is_input_text():
            # just create the red-circle plot.
            if small:
                (scale, dw, dh) = df.get_small_scale()
            else:
                (scale, dw, dh) = df.get_medium_scale()
            if scale == 1:
                if eightbit:
                    return convert(job, df, 'ppm-8bit')
                else:
                    return convert(job, df, 'ppm')
            xylist = job.get_filename('job.axy')
            cmd = ('plotxy -i %s -W %i -H %i -s %i -x 1 -y 1 -C brightred -w 2 -P > %s' %
                   (xylist, dw, dh, scale, fullfn))
            run_convert_command(cmd)
            return fullfn

        if small:
            imgfn = convert(job, df, 'pnm-small')
        else:
            imgfn = convert(job, df, 'pnm-medium')
        x = run_pnmfile(imgfn)
        if x is None:
            raise FileConversionError('pnmfile failed')
        (w, h, pnmtype, maxval) = x
        if pnmtype == 'P':
            if eightbit and (maxval != 255):
                cmd = 'pnmdepth 255 "%s" > "%s"' % (imgfn, fullfn)
                run_convert_command(cmd)
                return fullfn
            return imgfn
        if eightbit and (maxval != 255):
            cmd = 'pnmdepth 255 "%s" | pgmtoppm white > "%s"' % (imgfn, fullfn)
        else:
            cmd = 'pgmtoppm white %s > %s' % (imgfn, fullfn)
        run_convert_command(cmd)
        return fullfn

    elif fn == 'annotation':
        wcsfn = job.get_filename('wcs.fits')
        imgfn = convert(job, df, 'ppm-medium-8bit')
        (scale, dw, dh) = df.get_medium_scale()
        cmd = ('plot-constellations -N -w %s -o %s -C -B -b 10 -j -s %g -i %s' %
               (wcsfn, fullfn, 1.0/float(scale), imgfn))
        run_convert_command(cmd)
        return fullfn

    elif fn == 'annotation-big':
        imgfn = convert(job, df, 'ppm-8bit')
        wcsfn = job.get_filename('wcs.fits')
        cmd = ('plot-constellations -N -w %s -o %s -C -B -b 10 -j -i %s' %
               (wcsfn, fullfn, imgfn))
        run_convert_command(cmd)
        return fullfn

    elif fn == 'redgreen' or fn == 'redgreen-big':
        if fn == 'redgreen':
            imgfn = convert(job, df, 'ppm-medium-8bit')
            (dscale, dw, dh) = df.get_medium_scale()
            scale = 1.0 / float(dscale)
        else:
            imgfn = convert(job, df, 'ppm-8bit')
            scale = 1.0
        fxy = job.get_filename('job.axy')
        ixy = convert(job, df, 'index-xy')
        commonargs = ' -S %f -x %f -y %f -w 2' % (scale, scale, scale)
        logfn = 'blind.log'
        cmd = ('plotxy -i %s -I %s -r 4 -C green -P' % (ixy, imgfn) + commonargs 
               + '| plotxy -i %s -I - -P -r 6 -C brightred -N 50' % (fxy) + commonargs 
               + '| plotxy -i %s -I - -r 4 -C brightred -n 50 > %s' %
               (fxy, fullfn) + commonargs)
        run_convert_command(cmd, fullfn)
        return fullfn

    elif fn == 'index-xy':
        irdfn = job.get_filename('index.rd.fits')
        wcsfn = job.get_filename('wcs.fits')
        cmd = 'wcs-rd2xy -q -w %s -i %s -o %s' % (wcsfn, irdfn, fullfn)
        run_convert_command(cmd, fullfn)
        return fullfn

    elif fn == 'sources-small':
        imgfn = convert(job, df, 'ppm-small-8bit')
        if variant:
            fn = 'sources-small-%i' % variant
            fullfn = basename + fn
            if os.path.exists(fullfn):
                return fullfn
            xyls = convert(job, df, 'xyls', args)
        else:
            xyls = job.get_filename('job.axy')
        (dscale, dw, dh) = df.get_small_scale()
        scale = 1.0 / float(dscale)
        commonargs = ('-i %s -x %g -y %g -w 1 -S %g -C red' %
                      (xyls, scale, scale, scale))
        cmd = (('plotxy %s -I %s -N 100 -r 5 -P' %
                (commonargs, imgfn)) +
               (' | plotxy -I - %s -n 100 -N 500 -r 4 > %s' %
                (commonargs, fullfn)))
        log('Command: ' + cmd)
        run_convert_command(cmd, fullfn)
        return fullfn

    elif fn == 'sources-medium':
        imgfn = convert(job, df, 'ppm-medium-8bit')
        xyls = job.get_filename('job.axy')
        (dscale, dw, dh) = df.get_medium_scale()
        scale = 1.0 / float(dscale)
        commonargs = ('-i %s -x %g -y %g -w 2 -S %g -C red' %
                      (xyls, scale, scale, scale))
        cmd = (('plotxy %s -I %s -N 100 -r 6 -P' %
                (commonargs, imgfn)) +
               (' | plotxy -I - %s -n 100 -N 500 -r 4 > %s' %
                (commonargs, fullfn)))
        run_convert_command(cmd, fullfn)
        return fullfn

    elif fn == 'sources-big':
        imgfn = convert(job, df, 'ppm-8bit')
        xyls = job.get_filename('job.axy')
        commonargs = ('-i %s -x %g -y %g -w 2 -C red' %
                      (xyls, 1, 1))
        cmd = (('plotxy %s -I %s -N 100 -r 6 -P' %
                (commonargs, imgfn)) +
               (' | plotxy -I - %s -n 100 -N 500 -r 4 > %s' %
                (commonargs, fullfn)))
        log('Command: ' + cmd)
        run_convert_command(cmd, fullfn)
        return fullfn

    errmsg = 'Unimplemented: convert(%s)' % fn
    log(errmsg)
    raise FileConversionError(errmsg)

