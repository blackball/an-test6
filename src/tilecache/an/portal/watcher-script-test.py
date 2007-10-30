#! /usr/bin/env python

import sys
import os
import os.path

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print 'Usage: %s <ssh-config> <input-file>' % sys.argv[0]
        sys.exit(-1)

    sshconfig = sys.argv[1]
    jobfile = sys.argv[2]

    # jobfile should be a symlink; get the destination.
    link = os.readlink(jobfile)

    # the name of the jobfile is its jobid.
    jobid = os.path.basename(jobfile)

    # get the job directory.
    jobdir = os.path.dirname(link)

    # go to the job directory.
    os.chdir(jobdir)

    axyfile = os.path.basename(link)

    origfile = job.get_orig_file()

    logging.debug('Job: ' + str(job))

    if job.datasrc == 'url':
        # download the URL.
        #f = urllib.urlopen(url, 
        logging.debug('Retrieving URL...')
        f = urllib.urlretrieve(job.url, origfile)

    elif job.datasrc == 'file':
        # move the uploaded file.
        #uid = job.uploaded
        #if not uid:
        #return HttpResponse('no upload_id')
        temp = job.uploaded.get_filename()
        logging.debug('uploaded tempfile is ' + temp)
        os.rename(temp, origfile)

    else:
        return HttpResponse('no datasrc')

    # Handle compressed files.
    uncomp = create_file(job, 'uncomp', ['check-compressed'])

    # Compute hash of uncompressed file.
    job.compute_filehash(uncomp)

    axypath = None

    if job.filetype == 'image':
        # create fits image to feed to image2xy
        #fitsimg = create_file(job, 'fitsimg',
        #                      ['check-imgtype', 'save-imgsize'])
        #logging.debug('created fits image %s' % fitsimg)

        xylist = create_file(job, 'xyls',
                             ['check-imgtype', 'save-imgsize'])
        logging.debug('created xylist %s' % xylist)

        (lower, upper) = job.get_scale_bounds()

        axy = 'job.axy'

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

        jobdir = job.get_job_dir()
        os.chdir(jobdir)

        logging.debug('running: ' + cmd)
        (rtn, out, err) = run_command(cmd)
        if rtn:
            logging.debug('out: ' + out)
            logging.debug('err: ' + err)
            return HttpResponse('Creating axy file failed: ' + err)

        axypath = jobdir + '/' + axy

        logging.debug('created file ' + axypath)

        # shrink
        #job.displayscale = max(1, int(math.pow(2, math.ceil(math.log(max(w, h) / float(800)) / math.log(2)))))
        #job.displayw = int(round(w / float(job.displayscale)))
        #job.displayh = int(round(h / float(job.displayscale)))
        #if job.displayscale != 1:
        #    (f, smallppm) = tempfile.mkstemp()
        #    os.close(f)
        #    cmd = 'pnmscale -reduce %i %s > %s' % (job.displayscale, pnmfile, smallppm)
        #    logging.debug('command: ' + cmd)
        #    os.system(cmd)
        #    logging.debug('small ppm file %s' % smallppm)

    elif job.filetype == 'fits':
        return HttpResponse('fits tables not implemented')
        pass
    elif job.filetype == 'text':
        return HttpResponse('text files not implemented')
        pass
    else:
        return HttpResponse('no filetype')




    log = 'blind.log'

    # shell into compute server...
    cmd = ('(echo %(jobid)s;'
           'tar cf - --ignore-failed-read %(axyfile)s) | '
           'ssh -x -T %(sshconfig)s 2>>%(logfile)s | '
           'tar xf - --atime-preserve -m --exclude=%(axyfile)s '
           '>>%(logfile)s 2>&1)' %
           dict(jobid=jobid, axyfile=axyfile,
                sshconfig=sshconfig, logfile=logfile))
    
