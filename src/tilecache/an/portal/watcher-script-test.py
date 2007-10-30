#! /usr/bin/env python

import logging
import os
import os.path
import sys
import urllib

from django.db import models

import an.gmaps_config as config

from an.portal.models import Job
from an.upload.models import UploadedFile
from an.portal.log import log
from an.portal.convert import convert
from an.portal.run_command import run_command

def bailout(job, reason):
    job.status = 'Failed'
    job.failurereason = reason
    job.save()
    sys.exit(-1)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print 'Usage: %s <ssh-config> <input-file>' % sys.argv[0]
        sys.exit(-1)

    sshconfig = sys.argv[1]
    joblink = sys.argv[2]

    # jobfile should be a symlink; get the destination.
    jobdir = os.readlink(joblink)

    # the name of the jobfile is its jobid.
    jobid = os.path.basename(joblink)

    # go to the job directory.
    os.chdir(jobdir)

    job = Job.objects.all().filter(jobid=jobid)

    log('Running job: ' + str(job))
    if not job:
        sys.exit(-1)

    origfile = job.get_orig_file()

    if job.datasrc == 'url':
        # download the URL.
        log('Retrieving URL...')
        f = urllib.urlretrieve(job.url, origfile)

    elif job.datasrc == 'file':
        # move the uploaded file.
        temp = job.uploaded.get_filename()
        log('uploaded tempfile is ' + temp)
        os.rename(temp, origfile)

    else:
        bailout('no datasrc')

    # Handle compressed files.
    uncomp = convert(job, 'uncomp')

    # Compute hash of uncompressed file.
    job.compute_filehash(uncomp)

    axy = 'job.axy'
    axypath = job.get_filename(axy)

    if job.filetype == 'image':
        xylist = convert(job, 'xyls', store_imgtype=True, store_imgsize=True)
        log('created xylist %s' % xylist)
        (lower, upper) = job.get_scale_bounds()

        cmd = ('augment-xylist -x %s -o %s --solved solved '
               '--match match.fits --rdls index.rd.fits '
               '--wcs wcs.fits --sort-column FLUX '
               '--scale-units %s --scale-low %g --scale-high %g '
               '--fields 1' %
               (xylist, axy, job.scaleunits, lower, upper))
        if job.tweak:
            cmd += ' --tweak-order %i' % job.tweakorder
        else:
            cmd += ' --no-tweak'

        log('running: ' + cmd)
        (rtn, out, err) = run_command(cmd)
        if rtn:
            log('out: ' + out)
            log('err: ' + err)
            bailout('Creating axy file failed: ' + err)

        log('created file ' + axypath)

        # shrink
        #job.displayscale = max(1, int(math.pow(2, math.ceil(math.log(max(w, h) / float(800)) / math.log(2)))))
        #job.displayw = int(round(w / float(job.displayscale)))
        #job.displayh = int(round(h / float(job.displayscale)))
        #if job.displayscale != 1:
        #    (f, smallppm) = tempfile.mkstemp()
        #    os.close(f)
        #    cmd = 'pnmscale -reduce %i %s > %s' % (job.displayscale, pnmfile, smallppm)
        #    log('command: ' + cmd)
        #    os.system(cmd)
        #    log('small ppm file %s' % smallppm)

    elif job.filetype == 'fits':
        bailout('fits tables not implemented')
    elif job.filetype == 'text':
        bailout('text files not implemented')
    else:
        bailout('no filetype')

    job.save()

    blindlog = 'blind.log'
    # shell into compute server...
    cmd = ('(echo %(jobid)s; '
           'tar cf - --ignore-failed-read %(axyfile)s) | '
           'ssh -x -T %(sshconfig)s 2>>%(logfile)s | '
           'tar xf - --atime-preserve -m --exclude=%(axyfile)s '
           '>>%(logfile)s 2>&1)' %
           dict(jobid=jobid, axyfile=axyfile,
                sshconfig=sshconfig, logfile=blindlog))
    
    os.system(cmd)
