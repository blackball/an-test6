import datetime
import logging
import os.path
import random
import sha

from django.db import models
from django.contrib.auth.models import User

from an.upload.models import UploadedFile

import an.gmaps_config as config
from an.portal.log import log
from an.portal.wcs import *

from an.portal.models import UserPreferences


# Represents one file on disk
class DiskFile(models.Model):
    # sha-1 hash of the file contents.
    filehash = models.CharField(max_length=40, unique=True, primary_key=True)

    ### Everything below here can be derived from the above; they're
    ### just cached for performance reasons.

    # type of the file
    # ("jpg", "png", "gif", "fits", etc)
    filetype = models.CharField(max_length=16, null=True)

    # size of the image
    imagew = models.PositiveIntegerField(null=True)
    imageh = models.PositiveIntegerField(null=True)

    



class License(models.Model):
    pass

class Tag(models.Model):
    # Who added this tag?
    user = models.ForeignKey(User)

    # Machine tag or human-readable?
    machineTag = models.BooleanField(default=False)

    # The tag.
    text = models.CharField(max_length=4096)

    # When was this tag added?
    addedtime = models.DateTimeField()


class Calibration(models.Model):
    # TAN WCS, straight from the quad match
    raw_tan = models.ForeignKey(TanWCS, related_name='calibrations_raw', null=True)
    # TAN WCS, after tweaking
    tweaked_tan = models.ForeignKey(TanWCS, related_name='calibrations_tweaked', null=True)
    # SIP
    sip = models.ForeignKey(SipWCS, null=True)

    # RA,Dec bounding box.
    ramin =      models.FloatField()
    ramax =      models.FloatField()
    decmin = models.FloatField()
    decmax = models.FloatField()

    # in the future...
    #blind_date = models.DateField()
    # or
    #blind_date = models.ForeignKey(BlindDateSolution, null=True)

    # bandpass
    # zeropoint
    # psf



class BatchSubmission(models.Model):
    pass

class WebSubmission(models.Model):
    # All sorts of goodies like IP, HTTP headers, etc.
    pass

class Submission(models.Model):
    scaleunits_CHOICES = (
        ('arcsecperpix', 'arcseconds per pixel'),
        ('arcminwidth' , 'width of the field (in arcminutes)'), 
        ('degwidth' , 'width of the field (in degrees)'),
        ('focalmm'     , 'focal length of the lens (for 35mm film equivalent sensor)'),
        )
    scaleunits_default = 'degwidth'

    scaletype_CHOICES = (
        ('ul', 'lower and upper bounds'),
        ('ev', 'estimate and error bound'),
        )

    parity_CHOICES = (
        (2, 'Try both simultaneously'),
        (0, 'Positive'),
        (1, 'Negative'),
        )

    datasrc_CHOICES = (
        ('url', 'URL'),
        ('file', 'File'),
        )

    filetype_CHOICES = (
        ('image', 'Image (jpeg, png, gif, tiff, raw, or FITS)'),
        ('fits', 'FITS table of source locations'),
        ('text', 'Text list of source locations'),
        )
    ###

    subid = models.CharField(max_length=32, unique=True, primary_key=True)

    user = models.ForeignKey(User)

    # Only one of these should be set...
    batch = models.ForeignKey(BatchSubmission, null=True)
    web = models.ForeignKey(WebSubmission, null=True)

    # url / file / etc.
    datasrc = models.CharField(max_length=10, choices=datasrc_CHOICES)

    # image / fits / text
    filetype = models.CharField(max_length=10, choices=filetype_CHOICES)

    status = models.CharField(max_length=16)
    failurereason = models.CharField(max_length=256)

    url = models.URLField(blank=True, null=True)

    uploaded = models.ForeignKey(UploadedFile, null=True, blank=True)

    parity = models.PositiveSmallIntegerField(choices=parity_CHOICES,
                                              default=2, radio_admin=True,
                                              core=True)

    # for FITS tables, the names of the X and Y columns.
    xcol = models.CharField(max_length=16, blank=True)
    ycol = models.CharField(max_length=16, blank=True)

    # image scale.
    scaleunits = models.CharField(max_length=16, choices=scaleunits_CHOICES,
                                  default=scaleunits_default)
    scaletype  = models.CharField(max_length=3, choices=scaletype_CHOICES,
                                  default='ul')
    scalelower = models.FloatField(default=0.1, blank=True, null=True)
    scaleupper = models.FloatField(default=180, blank=True, null=True)
    scaleest   = models.FloatField(blank=True, null=True)
    scaleerr   = models.FloatField(blank=True, null=True)

    # tweak.
    tweak = models.BooleanField(default=True)
    tweakorder = models.PositiveSmallIntegerField(default=2)

    submittime = models.DateTimeField(null=True)

    def __init__(self, *args, **kwargs):
        for k,v in kwargs.items():
            if v is None:
                del kwargs[k]
        if not 'jobid' in kwargs:
            kwargs['jobid'] = Job.generate_jobid()
        super(Submission, self).__init__(*args, **kwargs)

    def __str__(self):
        s = '<Submission %s, status %s, user %s' % (self.jobid, self.status, self.user.username)
        if self.datasrc == 'url':
            s += ', url ' + str(self.url)
        elif self.datasrc == 'file':
            s += ', file "%s" (upload id %s)' % (self.uploaded.userfilename, str(self.uploaded))
        s += ', ' + self.filetype
        pstrs = [ 'pos', 'neg', 'both' ]
        s += ', parity ' + pstrs[int(self.parity)]
        if self.scaletype == 'ul':
            s += ', scale [%g, %g] %s' % (self.scalelower, self.scaleupper, self.scaleunits)
        elif self.scaletype == 'ev':
            s += ', scale [%g +- %g %%] %s' % (self.scaleest, self.scaleerr, self.scaleunits)
        if self.tweak:
            s += ', tweak order ' + str(self.tweakorder)
        else:
            s += ', no tweak'
        s += '>'
        return s

    def get_job_dir(self):
        return Job.s_get_job_dir(self.jobid)

    def get_relative_job_dir(self):
        return Job.s_get_relative_job_dir(self.jobid)

    def create_job_dir(self):
        Job.create_dir_for_jobid(self.jobid)

    def get_filename(self, fn):
        return Job.get_job_filename(self.jobid, fn)

    def get_url(self):
        if self.datasrc == 'url':
            return self.url
        return None

    def get_userfilename(self):
        if self.datasrc == 'file':
            return self.uploaded.userfilename
        return None

    def set_submittime_now(self):
        self.submittime = Job.timenow()
    def format_submittime(self):
        return Job.format_time(self.submittime)
    def format_submittime_brief(self):
        return Job.format_time_brief(self.submittime)


class Job(models.Model):
    # test-200802-12345678
    jobid = models.CharField(max_length=32, unique=True, primary_key=True)

    submission = models.ForeignKey(Submission, related_name='jobs', null=True)

    # The file that goes with this job
    diskfile = models.ForeignKey(DiskFile, null=True)

    # The license associated with the file.
    filelicense = models.ForeignKey(License, null=True)

    # The original filename on the user's machine, or basename of URL, etc.
    # Needed for, eg, tarball submissions.  Purely for informative purposes,
    # to show the user a filename that makes sense.
    fileorigname = models.CharField(max_length=64, null=True)

    # Has the user granted us permission to serve this file to everyone?
    exposefile = models.BooleanField(default=False)

    calibration = models.ForeignKey(Calibration, null=True)

    # Has the user granted us permission to show this job to everyone?
    exposejob = models.BooleanField(default=False)

    status = models.CharField(max_length=16)
    failurereason = models.CharField(max_length=256)

    # How did we find the calibration for this job?
    howsolved = models.CharField(max_length=256)

    # times
    starttime  = models.DateTimeField(null=True)
    finishtime = models.DateTimeField(null=True)

    tags = models.ManyToManyField(Tag)

    def __str__(self):
        s = '<Job %s, ' % self.jobid
        s += str(self.submission)
        if self.status:
            s += ', %s' % self.status
        s += ' ' + str(self.field)
        s += '>'
        return s

    def is_input_fits(self):
        return self.submission.filetype == 'fits'

    def is_input_text(self):
        return self.submission.filetype == 'text'

    def get_xy_cols(self):
        return (self.field.xcol, self.field.ycol)

    def friendly_parity(self):
        pstrs = [ 'Positive', 'Negative', 'Try both' ]
        return pstrs[int(self.get_parity())]

    def friendly_scale(self):
        val = None
        stype = self.get_scaletype()
        if stype == 'ul':
            val = '%.2f to %.2f' % (self.get_scalelower(), self.get_scaleupper())
        elif self.scaletype == 'ev':
            val = '%.2f plus or minus %.2f%%' % (self.get_scaleest(), self.get_scaleerr())

        txt = None
        units = self.get_scaleunits()
        if units == 'arcsecperpix':
            txt = val + ' arcseconds per pixel'
        elif units == 'arcminwidth':
            txt = val + ' arcminutes wide'
        elif units == 'degwidth':
            txt = val + ' degrees wide'
        elif units == 'focalmm':
            txt = 'focal length of ' + val + ' mm'
        return txt

    def get_scale_bounds(self):
        stype = self.get_scaletype()
        if stype == 'ul':
            return (self.get_scalelower(), self.get_scaleupper())
        elif stype == 'ev':
            est = self.get_scaleest()
            err = self.get_scaleerr()
            return (est * (1.0 - err / 100.0),
                    est * (1.0 + err / 100.0))
        else:
            return None

    def get_parity(self):
        if self.parity is not None:
            return self.parity
        return self.submission.parity

    def get_scalelower(self):
        return self.scalelower or self.submission.scalelower

    def get_scaleupper(self):
        return self.scaleupper or self.submission.scaleupper

    def get_scaleest(self):
        return self.scaleest or self.submission.scaleest

    def get_scaleerr(self):
        return self.scaleerr or self.submission.scaleerr

    def get_scaletype(self):
        return self.scaletype or self.submission.scaletype

    def get_scaleunits(self):
        return self.scaleunits or self.submission.scaleunits

    def get_tweak(self):
        if self.tweak is not None:
            tweak = self.tweak
        else:
            tweak = self.submission.tweak
        if self.tweakorder is not None:
            order = self.tweakorder
        else:
            order = self.submission.tweak
        return (tweak, order)

    def get_job_dir(self):
        return Job.s_get_job_dir(self.jobid)

    def get_relative_job_dir(self):
        return Job.s_get_relative_job_dir(self.jobid)

    def get_user(self):
        return self.submission.user

    def create_job_dir(self):
        Job.create_dir_for_jobid(self.jobid)

    def allowanonymous(self, prefs=None):
        if self.allowanon:
            return True
        if self.forbidanon:
            return False
        if not prefs:
            prefs = UserPreferences.for_user(self.submission.user)
        return prefs.anonjobstatus

    def set_starttime_now(self):
        self.starttime = Job.timenow()
    def set_finishtime_now(self):
        self.finishtime = Job.timenow()

    def format_starttime(self):
        return Job.format_time(self.starttime)
    def format_finishtime(self):
        return Job.format_time(self.finishtime)

    def format_starttime_brief(self):
        return Job.format_time_brief(self.starttime)
    def format_finishtime_brief(self):
        return Job.format_time_brief(self.finishtime)

    def get_orig_file(self):
        return self.field.filename()

    def get_filename(self, fn):
        return Job.get_job_filename(self.jobid, fn)

    def get_relative_filename(self, fn):
        return os.path.join(self.get_relative_job_dir(), fn)

    @staticmethod
    def timenow():
        return datetime.datetime.utcnow()

    @staticmethod
    def format_time(t):
        if not t:
            return None
        return t.strftime('%Y-%m-%d %H:%M:%S+Z')
    
    @staticmethod
    def format_time_brief(t):
        if not t:
            return None
        return t.strftime('%Y-%m-%d %H:%M')

    @staticmethod
    def create_dir_for_jobid(jobid):
        d = Job.s_get_job_dir(jobid)
        # HACK - more careful here...
        if os.path.exists(d):
            return
        mode = 0770
        os.makedirs(d, mode)

    @staticmethod
    def s_get_job_dir(jobid):
        return os.path.join(config.jobdir, Job.s_get_relative_job_dir(jobid))
    
    @staticmethod
    def get_job_filename(jobid, fn):
        return os.path.join(Job.s_get_job_dir(jobid), fn)

    @staticmethod
    def s_get_relative_job_dir(jobid):
        return os.path.join(*jobid.split('-'))

    @staticmethod
    def generate_jobid():
        today = datetime.date.today()
        jobid = '%s-%i%02i-%08i' % (config.siteid, today.year,
                                    today.month, random.randint(0, 99999999))
        return jobid

    @staticmethod
    def submit_job_or_submission(j):
        os.umask(07)
        j.create_job_dir()
        # enqueue by creating a symlink in the job queue directory.
        jobdir = j.get_job_dir()
        link = config.jobqueuedir + j.jobid
        os.symlink(jobdir, link)

