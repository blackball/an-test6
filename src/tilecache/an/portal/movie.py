import os
os.environ['DJANGO_SETTINGS_MODULE'] = 'an.settings'
os.environ['PATH'] = '/bin:/usr/bin:/home/gmaps/test/quads:/home/gmaps/test/tilecache/render'

import logging
import os.path
import shutil
import sys

from django.db import models

from an.portal.models import Job, Submission, DiskFile, Calibration, Tag
from an.portal.log import log
from an.portal.convert import convert, is_tarball, get_objs_in_field, FileConversionError
from an.util.run_command import run_command

# mencoder -o sky.avi -ovc lavc -lavcopts vcodec=mpeg4:keyint=50:autoaspect mf://sky-0*.png -mf fps=10:type=png
# mencoder -o ann.avi -ovc lavc -lavcopts vcodec=mpeg4:keyint=50:autoaspect mf://ann-*.png -mf fps=10:type=png

if __name__ == '__main__':

    subid = 'test-200804-28074176'
    outdir = '/tmp/movie/'

    blankskyfn = os.path.join(outdir, 'sky-blank.png')

    (ramin,ramax,decmin,decmax) = (0, 360, -85, 85)
    (w, h) = (300, 300)
    layers = [ 'tycho', 'grid' ]
    gain = -1
    cmd = ('tilerender'
           + ' -x %f -X %f -y %f -Y %f' % (ramin, ramax, decmin, decmax)
           + ' -w %i -h %i' % (w, h)
           + ''.join((' -l ' + l) for l in layers)
           + ' -s' # arcsinh
           + ' -g %g' % gain
           + ' > %s' % blankskyfn
           )
    (rtn, out, err) = run_command(cmd)
    if rtn:
        print 'Failed to tilerender the blank sky.'
        print 'out', out
        print 'err', err
        sys.exit(-1);


    subs = Submission.objects.all().filter(subid=subid)
    if len(subs) != 1:
        print 'Got %i submissions, not 1.' % len(subs)
        sys.exit(-1)
    sub = subs[0]
    jobs = sub.jobs.all().order_by('fileorigname')
    
    print 'Got %i jobs.' % len(jobs)

    ns = 0
    nu = 0
    for i, job in enumerate(jobs):
        if job.solved():
            ns += 1
            print 'Job %i: creating annotation.' % (i+1)
            annfn = convert(job, job.diskfile, 'annotation-big')
            print 'Job %i: creating on-the-sky.' % (i+1)
            skyfn = convert(job, job.diskfile, 'onsky-dot')
        else:
            nu += 1
            print 'Job %i: getting original.' % (i+1)
            annfn = convert(job, job.diskfile, 'fullsizepng')
            skyfn = None

        newannfn = os.path.join(outdir, 'ann-' + job.fileorigname)
        shutil.move(annfn, newannfn)

        newskyfn = os.path.join(outdir, 'sky-' + job.fileorigname)
        if skyfn:
            shutil.move(skyfn, newskyfn)
        else:
            os.symlink(blankskyfn, newskyfn)

    print '%i solved and %i unsolved.' % (ns, nu)
    
