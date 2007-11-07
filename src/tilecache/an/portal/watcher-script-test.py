#! /usr/bin/env python

import os
import sys
import tempfile

os.environ['DJANGO_SETTINGS_MODULE'] = 'an.settings'
sys.path.extend(['/home/gmaps/test/tilecache',
                 '/home/gmaps/test/an-common',
                 '/home/gmaps/test/',
                 '/home/gmaps/django/lib/python2.4/site-packages'])

import an.gmaps_config as config

os.environ['LD_LIBRARY_PATH'] = '/home/gmaps/test/an-common'
os.environ['PATH'] = '/bin:/usr/bin:/home/gmaps/test/quads'

import logging
import os.path
import urllib

from django.db import models

import an.gmaps_config as config

from an.portal.models import Job, JobSet, AstroField
from an.upload.models import UploadedFile
from an.portal.log import log
from an.portal.convert import convert, is_tarball, FileConversionError
from an.portal.run_command import run_command

def bailout(job, reason):
    job.status = 'Failed'
    job.failurereason = reason
    job.save()
    sys.exit(-1)

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

def handle_job(job):
    log('handle_job: ' + str(job))

    field = job.field
    log('field file is %s' % field.filename())

    jobset = job.jobset

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

    #log('PATH is ' + ', '.join(sys.path))

    filetype = job.jobset.filetype

    if filetype == 'image':
        log('source extraction...')
        userlog('Doing source extraction...')
        try:
            log('getting xylist...')
            xylist = convert(job, field, 'xyls', store_imgtype=True, store_imgsize=True)
            log('xylist is', xylist)
        except FileConversionError,e:
            userlog('Source extraction failed.')
            errlog = file_get_contents('blind.log')
            #if errlog.find('Shrink your image'):
            #userlog('Downsampling your image and trying again...')
            #xylist = convert(job, 'xyls-half')
            bailout(job, 'Source extraction failed.')
        log('created xylist %s' % xylist)
        (lower, upper) = jobset.get_scale_bounds()

        cmd = ('augment-xylist -x %s -o %s --solved solved '
               '--match match.fits --rdls index.rd.fits '
               '--wcs wcs.fits --sort-column FLUX '
               '--scale-units %s --scale-low %g --scale-high %g '
               '--fields 1' %
               (xylist, axy, jobset.scaleunits, lower, upper))
        if jobset.tweak:
            cmd += ' --tweak-order %i' % jobset.tweakorder
        else:
            cmd += ' --no-tweak'

        log('running: ' + cmd)
        (rtn, out, err) = run_command(cmd)
        if rtn:
            log('out: ' + out)
            log('err: ' + err)
            bailout(job, 'Creating axy file failed: ' + err)

        log('created file ' + axypath)

    elif filetype == 'fits':
        bailout(job, 'fits tables not implemented')
    elif filetype == 'text':
        bailout(job, 'text files not implemented')
    else:
        bailout(job, 'no filetype')

    field.save()

    # shell into compute server...
    cmd = ('(echo %(jobid)s; '
           ' tar cf - --ignore-failed-read %(axyfile)s) | '
           'ssh -x -T %(sshconfig)s 2>>%(logfile)s | '
           'tar xf - --atime-preserve -m --exclude=%(axyfile)s '
           '>>%(logfile)s 2>&1' %
           dict(jobid=jobid, axyfile=axy,
                sshconfig=sshconfig, logfile=blindlog))

    job.status = 'Running'
    job.set_starttime_now()
    job.save()

    log('Running command:', cmd)
    w = os.system(cmd)

    job.set_finishtime_now()
    job.save()

    if not os.WIFEXITED(w):
        bailout(job, 'Solver didn\'t exit normally.')

    rtn = os.WEXITSTATUS(w)
    if rtn:
        log('Solver failed with return value %i' % rtn)
        bailout(job, 'Solver failed.')

    log('Command completed successfully.')

    # Record results in the job database.

    if os.path.exists(job.get_filename('solved')):
        job.solved = True
        job.status = 'Solved'
    else:
        job.status = 'Failed'
        job.failurereason = 'Did not solve.'

    job.save()


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

    jobs = Job.objects.all().filter(jobid = jobid)
    if len(jobs):
        handle_job(jobs[0])
        sys.exit(0)

    jobsets = JobSet.objects.all().filter(jobid=jobid)
    if len(jobsets) != 1:
        log('Found %i jobsets, not 1' % len(jobsets))
        sys.exit(-1)
    jobset = jobsets[0]
    log('Running jobset: ' + str(jobset))
    if not jobset:
        sys.exit(-1)

    field = AstroField(user = jobset.user,
                       xcol = jobset.xcol,
                       ycol = jobset.ycol,
                       )
    # ??
    field.save()
    origfile = field.filename()

    if jobset.datasrc == 'url':
        # download the URL.
        userlog('Retrieving URL...')
        f = urllib.urlretrieve(jobset.url, origfile)
    elif jobset.datasrc == 'file':
        # move the uploaded file.
        temp = jobset.uploaded.get_filename()
        log('uploaded tempfile is ' + temp)
        log('rename(%s, %s)' % (temp, origfile))
        os.rename(temp, origfile)
    else:
        bailout(job, 'no datasrc')

    # Handle compressed files.
    uncomp = convert(jobset, field, 'uncomp')

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

        log('Got %i paths.' % len(validpaths))

        for p in validpaths:
            field = AstroField(user = jobset.user,
                               xcol = jobset.xcol,
                               ycol = jobset.ycol,
                               )
            field.save()
            log('New field ' + str(field.id))
            destfile = field.filename()
            os.rename(p, destfile)
            log('Moving %s to %s' % (p, destfile))

            if len(validpaths) == 1:
                job = Job(
                    jobid = Job.generate_jobid(),
                    #jobid = jobset.jobid,
                    jobset = jobset,
                    field = field,
                    )
                job.create_job_dir()
                job.save()
                # One file in tarball: convert straight to a Job.
                log('Single-file tarball.')
                handle_job(job)
                sys.exit(0)

            job = Job(jobset = jobset,
                      field = field,
                      jobid = Job.generate_jobid(),
                      )
            os.umask(07)
            job.create_job_dir()
            job.set_submittime_now()
            job.status = 'Queued'
            job.save()
            # HACK - duplicate code from newjob.submit_jobset()
            log('Enqueuing Job: ' + str(job))
            jobdir = job.get_job_dir()
            link = gmaps_config.jobqueuedir + job.jobid
            os.symlink(jobdir, link)

    else:
        # Not a tarball.
        job = Job(
            #jobid = jobset.jobid,
            jobid = Job.generate_jobid(),
            jobset = jobset,
            field = field,
            )
        job.create_job_dir()
        job.save()
        handle_job(job)

    sys.exit(0)


