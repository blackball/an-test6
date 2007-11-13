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


# Represents one field to be solved: either an image or xylist.
class AstroField(models.Model):
    user = models.ForeignKey(User, editable=False)

    # Has the user explicitly granted us permission to redistribute this image?
    allowredist = models.BooleanField(default=False)
    # Has the user explicitly forbidden us permission to redistribute this image?
    forbidredist = models.BooleanField(default=False)

    # The original filename on the user's machine, or basename of URL, etc.
    # Needed for, eg, tarball jobsets.
    origname = models.CharField(max_length=64, null=True)

    # type of the uploaded file, after decompression
    # ("jpg", "png", "gif", "fits", etc)
    imgtype = models.CharField(max_length=16, editable=False)

    # sha-1 hash of the uncompressed file.
    filehash = models.CharField(max_length=40, editable=False)

    # for FITS tables, the names of the X and Y columns.
    xcol = models.CharField(max_length=16, blank=True)
    ycol = models.CharField(max_length=16, blank=True)

    # size of the image
    imagew = models.PositiveIntegerField(editable=False, null=True)
    imageh = models.PositiveIntegerField(editable=False, null=True)

    # shrink factor (= 1 / (scale factor)) for rendering images; >=1.
    displayscale = models.FloatField(editable=False, null=True)
    # size to render images.
    displayw = models.PositiveIntegerField(editable=False, null=True)
    displayh = models.PositiveIntegerField(editable=False, null=True)

    def __str__(self):
        s = '<Field ' + str(self.id)
        if self.imgtype:
            s += ', ' + self.imgtype
        s += '>'
        return s

    def content_type(self):
        typemap = {
            'jpg' : 'image/jpeg',
            'gif' : 'image/gif',
            'png' : 'image/png',
            'fits' : 'image/fits'
            }
        if not self.imgtype in typemap:
            return None
        return typemap[self.imgtype]

    def redistributable(self, prefs=None):
        if self.allowredist:
            return True
        if self.forbidredist:
            return False
        if not prefs:
            prefs = UserPreferences.for_user(self.user)
        return prefs.autoredistributable

    def compute_filehash(self, fn):
        if self.filehash:
            return
        h = sha.new()
        f = open(fn, 'rb')
        while True:
            d = f.read(4096)
            if len(d) == 0:
                break
            h.update(d)
        self.filehash = h.hexdigest()

    def filename(self):
        return os.path.join(config.fielddir, str(self.id))




class JobSet(models.Model):
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

    #datasrc_extra_CHOICES = (
    #    ('field', 'Existing field'),
    #    )

    filetype_CHOICES = (
        ('image', 'Image (jpeg, png, gif, tiff, or FITS)'),
        ('fits', 'FITS table of source locations'),
        ('text', 'Text list of source locations'),
        )

    jobid = models.CharField(max_length=32, unique=True, editable=False,
                             primary_key=True)

    #status_CHOICES = (
    #    ('not-submitted', 'Submission failed'),
    #    ('queued', 'Queued'),
    #    ('started', 'Started'),
    #    ('solved', 'Solved'),
    #    ('failed', 'Failed')
    #    )

    #
    status = models.CharField(max_length=16,
                              #choices=status_CHOICES,
                              editable=False)
    failurereason = models.CharField(max_length=256, editable=False)

    user = models.ForeignKey(User, editable=False)

    filetype = models.CharField(max_length=10, choices=filetype_CHOICES)

    datasrc = models.CharField(max_length=10,
                               choices=datasrc_CHOICES #+ datasrc_extra_CHOICES
                               )

    url = models.URLField(blank=True, null=True)

    uploaded = models.ForeignKey(UploadedFile, null=True, blank=True, editable=False)

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

    submittime = models.DateTimeField(editable=False, null=True)

    #status = models.CharField(max_length=16, editable=False)
    #failurereason = models.CharField(max_length=256, editable=False)

    def __init__(self, *args, **kwargs):
        for k,v in kwargs.items():
            if v is None:
                del kwargs[k]
        if not 'jobid' in kwargs:
            kwargs['jobid'] = Job.generate_jobid()
        super(JobSet, self).__init__(*args, **kwargs)

    def __str__(self):
        s = '<JobSet %s, status %s, user %s' % (self.jobid, self.status, self.user.username)
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

    def get_scale_bounds(self):
        if self.scaletype == 'ul':
            return (self.scalelower, self.scaleupper)
        elif self.scaletype == 'ev':
            return (self.scaleest * (1 - self.scaleerr / 100.0),
                    self.scaleest * (1 + self.scaleerr / 100.0))
        else:
            return None

    def friendly_parity(self):
        pstrs = [ 'Positive', 'Negative', 'Try both' ]
        return pstrs[int(self.parity)]

    def friendly_scale(self):
        val = None
        if self.scaletype == 'ul':
            val = '%.2f to %.2f' % (self.scalelower, self.scaleupper)
        elif self.scaletype == 'ev':
            val = '%.2f plus or minus %.2f%%' % (self.scaleest, self.scaleerr)

        txt = None
        if self.scaleunits == 'arcsecperpix':
            txt = val + ' arcseconds per pixel'
        elif self.scaleunits == 'arcminwidth':
            txt = val + ' arcminutes wide'
        elif self.scaleunits == 'degwidth':
            txt = val + ' degrees wide'
        elif self.scaleunits == 'focalmm':
            txt = 'focal length of ' + val + ' mm'
        return txt

    def set_submittime_now(self):
        self.submittime = Job.timenow()
    def format_submittime(self):
        return Job.format_time(self.submittime)
    def format_submittime_brief(self):
        return Job.format_time_brief(self.submittime)


class Job(models.Model):
    jobid = models.CharField(max_length=32, unique=True, editable=False,
                             primary_key=True)

    status = models.CharField(max_length=16,
                              editable=False)
    failurereason = models.CharField(max_length=256, editable=False)

    jobset = models.ForeignKey(JobSet, related_name='jobs',
                               null=True)

    # has the user explicitly granted anonymous access to this job
    # status page?
    allowanon = models.BooleanField(default=False)

    # has the user explicitly forbidden anonymous access to this job
    # status page?
    forbidanon = models.BooleanField(default=False)

    field = models.ForeignKey(AstroField, editable=False)

    # times
    starttime  = models.DateTimeField(editable=False, null=True)
    finishtime = models.DateTimeField(editable=False, null=True)

    # solution
    tanwcs = models.ForeignKey(TanWCS, null=True, editable=False)

    solved = models.BooleanField(default=False)

    def __str__(self):
        s = '<Job %s, ' % self.jobid
        s += str(self.jobset)
        if self.status:
            s += ', %s' % self.status
        s += ' ' + str(self.field)
        s += '>'
        return s

    def get_job_dir(self):
        return Job.s_get_job_dir(self.jobid)

    def get_relative_job_dir(self):
        return Job.s_get_relative_job_dir(self.jobid)

    def create_job_dir(self):
        Job.create_dir_for_jobid(self.jobid)

    def allowanonymous(self, prefs=None):
        if self.allowanon:
            return True
        if self.forbidanon:
            return False
        if not prefs:
            prefs = UserPreferences.for_user(self.jobset.user)
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

    def timenow():
        return datetime.datetime.utcnow()
    timenow = staticmethod(timenow)

    def format_time(t):
        if not t:
            return None
        return t.strftime('%Y-%m-%d %H:%M:%S+Z')
    format_time = staticmethod(format_time)
    
    def format_time_brief(t):
        if not t:
            return None
        return t.strftime('%Y-%m-%d %H:%M')
    format_time_brief = staticmethod(format_time_brief)

    def create_dir_for_jobid(jobid):
        d = Job.s_get_job_dir(jobid)
        # HACK - more careful here...
        if os.path.exists(d):
            return
        mode = 0770
        os.makedirs(d, mode)
    create_dir_for_jobid = staticmethod(create_dir_for_jobid)

    def s_get_job_dir(jobid):
        return os.path.join(config.jobdir, Job.s_get_relative_job_dir(jobid))
    s_get_job_dir = staticmethod(s_get_job_dir)
    
    def get_job_filename(jobid, fn):
        return os.path.join(Job.s_get_job_dir(jobid), fn)
    get_job_filename = staticmethod(get_job_filename)

    def s_get_relative_job_dir(jobid):
        return os.path.join(*jobid.split('-'))
    s_get_relative_job_dir = staticmethod(s_get_relative_job_dir)

    def generate_jobid():
        today = datetime.date.today()
        jobid = '%s-%i%02i-%08i' % (config.siteid, today.year,
                                    today.month, random.randint(0, 99999999))
        return jobid
    generate_jobid = staticmethod(generate_jobid)


    def submit_job_or_jobset(j):
        os.umask(07)
        j.create_job_dir()
        # enqueue by creating a symlink in the job queue directory.
        jobdir = j.get_job_dir()
        link = config.jobqueuedir + j.jobid
        os.symlink(jobdir, link)
    submit_job_or_jobset = staticmethod(submit_job_or_jobset)
