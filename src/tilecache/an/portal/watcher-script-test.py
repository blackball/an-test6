#! /usr/bin/env python

import os
import sys
import tempfile
import traceback

from urlparse import urlparse

os.environ['DJANGO_SETTINGS_MODULE'] = 'an.settings'
sys.path.extend(['/home/gmaps/test/tilecache',
                 '/home/gmaps/test/an-common',
                 '/home/gmaps/test',
                 '/home/gmaps/django/lib/python2.4/site-packages'])

import an.gmaps_config as config

os.environ['LD_LIBRARY_PATH'] = '/home/gmaps/test/an-common'
os.environ['PATH'] = '/bin:/usr/bin:/home/gmaps/test/quads'

import logging
import os.path
import urllib

from django.db import models

from an.portal.models import Job, JobSet, AstroField
from an.upload.models import UploadedFile
from an.portal.log import log
from an.portal.convert import convert, is_tarball, FileConversionError
from an.portal.wcs import TanWCS
from an.util.run_command import run_command

# HACK
import sip

def bailout(job, reason):
    job.status = 'Failed'
    job.failurereason = reason
    job.save()

blindlog = 'blind.log'

def file_get_contents(fn):
    f = open(fn, 'r')
    txt = f.read()
    f.close()
    return txt

def userlog(*msg):
    f = open(blindlog, 'a')
    f.write(' '.join(map(str, msg)) + '\n')
    f.close()

def handle_job(job, sshconfig):
    try:
        real_handle_job(job, sshconfig)
    except Exception,e:
        job.status = 'Failed'
        errstr = str(e)
        job.failurereason = errstr[:256]
        job.save()
        log('Failed with exception: ', str(e))
        log('--------------')
        log(traceback.format_exc())
        log('--------------')
   
def real_handle_job(job, sshconfig):
    log('handle_job: ' + str(job))

    job.status = 'Running'
    job.save()

    field = job.field
    log('field file is %s' % field.filename())

    jobset = job.jobset
    jobid = job.jobid

    # go to the job directory.
    jobdir = job.get_job_dir()
    os.chdir(jobdir)

    # Handle compressed files.
    uncomp = convert(job, field, 'uncomp')
    log('uncompressed file is %s' % uncomp)

    # Compute hash of uncompressed file.
    field.compute_filehash(uncomp)

    axy = 'job.axy'
    axypath = job.get_filename(axy)

    filetype = job.jobset.filetype

    axyargs = {}

    if filetype == 'image':
        log('source extraction...')
        userlog('Doing source extraction...')

        convert(job, field, 'getimagesize')
        if (field.imagew * field.imageh) > 5000000:  # 5 MPixels
            userlog('Downsampling your image...')
            target = 'xyls-half-sorted'
        else:
            target = 'xyls-sorted'

        try:
            log('image2xy...')
            xylist = convert(job, field, target)
            log('xylist is', xylist)
        except FileConversionError,e:
            userlog('Source extraction failed.')
            bailout(job, 'Source extraction failed.')
            return -1
        log('created xylist %s' % xylist)

        axyargs['-x'] = xylist

    elif (filetype == 'fits') or (filetype == 'text'):
        if filetype == 'text':
            field.filetype = 'text'
            try:
                userlog('Parsing your text file...')
                xylist = convert(job, field, 'xyls')
                log('xylist is', xylist)
            except FileConversionError,e:
                userlog('Parsing your text file failed.')
                bailout(job, 'Parsing text file failed.')
                return -1

        else:
            field.filetype = 'xyls'
            try:
                log('fits2fits...')
                xylist = convert(job, field, 'xyls')
                log('xylist is', xylist)
            except FileConversionError,e:
                userlog('Sanitizing your FITS file failed.')
                bailout(job, 'Sanitizing FITS file failed.')
                return -1

            (xcol, ycol) = job.get_xy_cols()
            if xcol:
                axyargs['--x-column'] = xcol
            if ycol:
                axyargs['--y-column'] = ycol

        log('created xylist %s' % xylist)

        cmd = 'xylsinfo %s' % xylist
        (rtn, out, err) = run_command(cmd)
        if rtn:
            log('out: ' + out)
            log('err: ' + err)
            bailout(job, 'Getting xylist image size failed: ' + err)
            return -1
        lines = out.strip().split('\n')
        #log('out: ', out)
        #log('lines: ', str(lines))
        info = {}
        for l in lines:
            t = l.split(' ')
            info[t[0]] = t[1]
        #log('info: ', str(info))
        if 'imagew' in info:
            width = float(info['imagew'])
        else:
            width = float(info['width'])
        if 'imageh' in info:
            height = float(info['imageh'])
        else:
            height = float(info['height'])

        axyargs.update({
            '-x': xylist,
            '--width' : width,
            '--height' : height,
            '--no-fits2fits' : None,
            })
        field.imagew = width
        field.imageh = height

    else:
        bailout(job, 'no filetype')
        return -1


    (lower, upper) = job.get_scale_bounds()
    units = job.get_scaleunits()

    axyargs.update({
        '-o' : axy,
        '--scale-units' : units,
        '--scale-low' : lower,
        '--scale-high' : upper,
        '--fields' : 1,
        '--wcs' : 'wcs.fits',
        '--rdls' : 'index.rd.fits',
        '--match' : 'match.fits',
        '--solved' : 'solved',
        })

    (dotweak, tweakorder) = job.get_tweak()
    log('do tweak?', dotweak, 'order', tweakorder)
    if dotweak:
        axyargs['--tweak-order'] = tweakorder
    else:
        axyargs['--no-tweak'] = None

    #for (k,v) in axyargs.items():
    #log('  ', k, ' = ', str(v))

    cmd = 'augment-xylist ' + ' '.join(k + ((v and ' ' + str(v)) or '') for (k,v) in axyargs.items())

    log('running: ' + cmd)
    (rtn, out, err) = run_command(cmd)
    if rtn:
        log('out: ' + out)
        log('err: ' + err)
        bailout(job, 'Creating axy file failed: ' + err)
        return -1

    log('created axy file ' + axypath)

    field.save()

    # shell into compute server...

    # For the "test" instance:
    #  ssh an_remote_test  (as gmaps)
    #  runs cluster24:amd-an-2/quads/an-remote-test.sh
    #  run backend with amd-an-2/quads/backend-test.cfg

    cmd = ('(echo %(jobid)s; '
           ' tar cf - --ignore-failed-read %(axyfile)s) | '
           'ssh -x -T %(sshconfig)s 2>>%(logfile)s | '
           'tar xf - --atime-preserve -m --exclude=%(axyfile)s '
           '>>%(logfile)s 2>&1' %
           dict(jobid=jobid, axyfile=axy,
                sshconfig=sshconfig, logfile=blindlog)
           + '; chmod 664 *; chgrp www-data *')
    # --group G --mode M --owner O ?

    job.status = 'Running'
    job.set_starttime_now()
    job.save()

    log('Running command:', cmd)
    w = os.system(cmd)

    job.set_finishtime_now()
    job.save()


    if not os.WIFEXITED(w):
        bailout(job, 'Solver didn\'t exit normally.')
        return -1

    rtn = os.WEXITSTATUS(w)
    if rtn:
        log('Solver failed with return value %i' % rtn)
        bailout(job, 'Solver failed.')
        return -1

    log('Command completed successfully.')

    # Record results in the job database.

    if os.path.exists(job.get_filename('solved')):
        job.solved = True
        job.status = 'Solved'

        # BIG HACK! - look through LD_LIBRARY_PATH if this is still needed...
        if not sip.libraryloaded():
            sip.loadlibrary('/home/gmaps/test/an-common/_sip.so')

        # Add WCS to database.
        wcs = TanWCS(file=job.get_filename('wcs.fits'))
        wcs.save()
        job.tanwcs = wcs
        
    else:
        job.status = 'Failed'
        job.failurereason = 'Did not solve.'
    job.save()


def main(sshconfig, joblink):
    if not os.path.islink(joblink):
        log('Expected second argument to be a symlink; "%s" isn\'t.' % joblink)
        return -1

    # jobfile should be a symlink; get the destination.
    jobdir = os.readlink(joblink)

    # go to the job directory.
    os.chdir(jobdir)

    # the name of the symlink is the jobid.
    jobid = os.path.basename(joblink)

    # if it's a Job...
    jobs = Job.objects.all().filter(jobid = jobid)
    if len(jobs):
        return handle_job(jobs[0], sshconfig)

    # else it's a JobSet...
    jobsets = JobSet.objects.all().filter(jobid=jobid)
    if len(jobsets) != 1:
        log('Found %i jobsets, not 1' % len(jobsets))
        sys.exit(-1)
    jobset = jobsets[0]
    log('Running jobset: ' + str(jobset))

    field = AstroField(user = jobset.user)
    # save() so the "id" field gets a unique value
    field.save()
    origfile = field.filename()
    basename = None

    if jobset.datasrc == 'url':
        # download the URL.
        userlog('Retrieving URL...')
        log('Retrieving URL ' + jobset.url + ' to file ' + origfile)
        f = urllib.urlretrieve(jobset.url, origfile)
        p = urlparse(jobset.url)
        p = p[2]
        if p:
            s = p.split('/')
            basename = s[-1]
    elif jobset.datasrc == 'file':
        # move the uploaded file.
        temp = jobset.uploaded.get_filename()
        log('uploaded tempfile is ' + temp)
        log('rename(%s, %s)' % (temp, origfile))
        os.rename(temp, origfile)
        basename = jobset.uploaded.userfilename
    else:
        bailout(job, 'no datasrc')
        return -1

    field.origname = basename
    field.save()

    # Handle compressed files.
    uncomp = convert(jobset, field, 'uncomp-js')

    # Handle tar files: add a JobSet, create new Jobs.
    job = None
    if is_tarball(uncomp):
        log('file is tarball.')
        # create temp dir to extract tarfile.
        tempdir = tempfile.mkdtemp()
        cmd = 'tar xvf %s -C %s' % (uncomp, tempdir)
        userlog('Extracting tarball...')
        (rtn, out, err) = run_command(cmd)
        if rtn:
            userlog('Failed to un-tar file:\n' + err)
            bailout(jobset, 'failed to extract tar file')
            return -1
        fns = out.strip('\n').split('\n')
        validpaths = []
        for fn in fns:
            path = os.path.join(tempdir, fn)
            log('Path "%s"' % path)
            if not os.path.exists(path):
                log('Path "%s" does not exist.' % path)
                continue
            if os.path.islink(path):
                log('Path "%s" is a symlink.' % path)
                continue
            if os.path.isfile(path):
                validpaths.append(path)
            else:
                log('Path "%s" is not a file.' % path)

        if len(validpaths) == 0:
            userlog('Tar file contains no regular files.')
            bailout(jobset, "tar file contains no regular files.")
            return -1

        log('Got %i paths.' % len(validpaths))

        for p in validpaths:
            field = AstroField(user = jobset.user,
                               origname = os.path.basename(p),
                               )
            field.save()
            log('New field ' + str(field.id))
            destfile = field.filename()
            os.rename(p, destfile)
            log('Moving %s to %s' % (p, destfile))

            if len(validpaths) == 1:
                job = Job(
                    jobid = jobset.jobid,
                    jobset = jobset,
                    field = field,
                    )
                job.save()
                jobset.save()
                # One file in tarball: convert straight to a Job.
                log('Single-file tarball.')
                rtn = handle_job(job, sshconfig)
                if rtn:
                    return rtn
                break

            job = Job(jobset = jobset,
                      field = field,
                      jobid = Job.generate_jobid(),
                      )
            job.status = 'Queued'
            job.save()
            jobset.save()
            log('Enqueuing Job: ' + str(job))
            Job.submit_job_or_jobset(job)

    else:
        # Not a tarball.
        job = Job(
            jobid = jobset.jobid,
            jobset = jobset,
            field = field,
            )
        job.save()
        jobset.save()
        rtn = handle_job(job, sshconfig)
        if rtn:
            return rtn

    # remove the symlink to indicate that we've successfully finished this
    # job.
    os.unlink(joblink)
    return 0


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print 'Usage: %s <ssh-config> <input-file>' % sys.argv[0]
        sys.exit(-1)
    sshconfig = sys.argv[1]
    joblink = sys.argv[2]

    os.umask(07)

    sys.exit(main(sshconfig, joblink))

