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

    # ID number of the file on the filesystem.  This is NOT necessarily
    # unique (multiple AstroFields may share a file).
    fileid = models.PositiveIntegerField(editable=False, null=True)

    # Has the user explicitly granted us permission to redistribute this image?
    allowredist = models.BooleanField(default=False)
    # Has the user explicitly forbidden us permission to redistribute this image?
    forbidredist = models.BooleanField(default=False)

    # The original filename on the user's machine, or basename of URL, etc.
    # Needed for, eg, tarball jobsets.  Purely for informative purposes,
    # to show the user a filename that makes sense.
    origname = models.CharField(max_length=64, null=True)

    # type of the uploaded file, after decompression
    # ("jpg", "png", "gif", "fits", etc)
    filetype = models.CharField(max_length=16, editable=False)

    # sha-1 hash of the uncompressed file.
    filehash = models.CharField(max_length=40, editable=False)

    # size of the image
    imagew = models.PositiveIntegerField(editable=False, null=True)
    imageh = models.PositiveIntegerField(editable=False, null=True)

    def __init__(self, *args, **kwargs):
        super(AstroField, self).__init__(*args, **kwargs)
        # ???
        if not self.fileid:
            self.fileid = self.id

    def __str__(self):
        s = '<Field ' + str(self.id)
        s += ', fileid ' + str(self.fileid)
        if self.filetype:
            s += ', ' + self.filetype
        s += '>'
        return s

    def save(self):
        # FIXME??
        # if "fileid" is not set, it gets the value of "id".
        # (but "id" may not get set until after save())
        super(AstroField, self).save()
        if not self.fileid:
            self.fileid = self.id
            super(AstroField, self).save()

    def content_type(self):
        typemap = {
            'jpg' : 'image/jpeg',
            'gif' : 'image/gif',
            'png' : 'image/png',
            'fits' : 'image/fits',
            'text' : 'text/plain',
            'xyls' : 'image/fits', # ??
            }
        if not self.filetype in typemap:
            return None
        return typemap[self.filetype]

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
        return os.path.join(config.fielddir, str(self.fileid))

    def get_display_scale(self):
        w = self.imagew
        h = self.imageh
        scale = float(max(1.0,
                          math.pow(2, math.ceil(
            math.log(max(w, h) / float(800)) / math.log(2)))))
        displayw = int(round(w / scale))
        displayh = int(round(h / scale))
        return (scale, displayw, displayh)

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

    filetype_CHOICES = (
        ('image', 'Image (jpeg, png, gif, tiff, or FITS)'),
        ('fits', 'FITS table of source locations'),
        ('text', 'Text list of source locations'),
        )

    jobid = models.CharField(max_length=32, unique=True, primary_key=True)

    user = models.ForeignKey(User)

    datasrc = models.CharField(max_length=10, choices=datasrc_CHOICES)

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

    def set_submittime_now(self):
        self.submittime = Job.timenow()
    def format_submittime(self):
        return Job.format_time(self.submittime)
    def format_submittime_brief(self):
        return Job.format_time_brief(self.submittime)


class Job(models.Model):
    jobid = models.CharField(max_length=32, unique=True, primary_key=True)

    status = models.CharField(max_length=16)
    failurereason = models.CharField(max_length=256)

    jobset = models.ForeignKey(JobSet, related_name='jobs', null=True)

    # has the user explicitly granted anonymous access to this job
    # status page?
    allowanon = models.BooleanField(default=False)

    # has the user explicitly forbidden anonymous access to this job
    # status page?
    forbidanon = models.BooleanField(default=False)

    field = models.ForeignKey(AstroField)


    # If any of these solver parameters are set for a particular Job, they
    # override the settings for the JobSet.
    parity = models.PositiveSmallIntegerField(choices=JobSet.parity_CHOICES, null=True)
    # for FITS tables, the names of the X and Y columns.
    xcol = models.CharField(max_length=16, null=True)
    ycol = models.CharField(max_length=16, null=True)
    # image scale.
    scaleunits = models.CharField(max_length=16, choices=JobSet.scaleunits_CHOICES, null=True)
    scaletype  = models.CharField(max_length=3, choices=JobSet.scaletype_CHOICES, null=True)
    scalelower = models.FloatField(null=True)
    scaleupper = models.FloatField(null=True)
    scaleest   = models.FloatField(null=True)
    scaleerr   = models.FloatField(null=True)
    # tweak.
    tweak = models.BooleanField(null=True)
    tweakorder = models.PositiveSmallIntegerField(null=True)


    # times
    starttime  = models.DateTimeField(null=True)
    finishtime = models.DateTimeField(null=True)

    # solution
    tanwcs = models.ForeignKey(TanWCS, null=True)

    solved = models.BooleanField(default=False)

    def __str__(self):
        s = '<Job %s, ' % self.jobid
        s += str(self.jobset)
        if self.status:
            s += ', %s' % self.status
        s += ' ' + str(self.field)
        s += '>'
        return s

    def is_input_fits(self):
        return self.jobset.filetype == 'fits'

    def is_input_text(self):
        return self.jobset.filetype == 'text'

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
        return self.jobset.parity

    def get_scalelower(self):
        return self.scalelower or self.jobset.scalelower

    def get_scaleupper(self):
        return self.scaleupper or self.jobset.scaleupper

    def get_scaleest(self):
        return self.scaleest or self.jobset.scaleest

    def get_scaleerr(self):
        return self.scaleerr or self.jobset.scaleerr

    def get_scaletype(self):
        return self.scaletype or self.jobset.scaletype

    def get_scaleunits(self):
        return self.scaleunits or self.jobset.scaleunits

    def get_tweak(self):
        if self.tweak is not None:
            tweak = self.tweak
        else:
            tweak = self.jobset.tweak
        if self.tweakorder is not None:
            order = self.tweakorder
        else:
            order = self.jobset.tweak
        return (tweak, order)

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
