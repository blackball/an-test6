import datetime
import logging
import os.path
import random
import re
import sha
import stat
import time

from django.db import models
from django.contrib.auth.models import User
from django.core import validators

import an.gmaps_config as config
from an.portal.log import log

from an.upload.models import UploadedFile
#from an_common import sip
import sip

# To install a new database table:
# > python manage.py sql portal
# > sqlite3 /data2/test-django/tile.db
# sqlite> drop table portal_job;
# sqlite>   (paste CREATE TABLE statement)

class UserPreferences(models.Model):
    user = models.ForeignKey(User, editable=False)

    # Automatically grant permission to redistribute my images?
    autoredistributable = models.BooleanField(default=False)

    # Automatically allow anonymous access to my job status pages?
    anonjobstatus = models.BooleanField(default=False)


    def for_user(user):
        prefset = UserPreferences.objects.all().filter(user = user)
        if not prefset or not len(prefset):
            # no existing user prefs.
            prefs = UserPreferences(user = user)
        else:
            prefs = prefset[0]
    for_user = staticmethod(for_user)

class TanWCS(models.Model):
    crval1 = models.FloatField()
    crval2 = models.FloatField()
    crpix1 = models.FloatField()
    crpix2 = models.FloatField()
    cd11 = models.FloatField()
    cd12 = models.FloatField()
    cd21 = models.FloatField()
    cd22 = models.FloatField()
    imagew = models.FloatField()
    imageh = models.FloatField()

class  SipWCS(TanWCS):
    order = models.PositiveSmallIntegerField(default=2)
    terms = models.TextField(default='')
    #def to_python(self, value):
    #vals = []
    #for i in range(self.order+1):
    #    for j in range(self.order+1):
    #        if i + j <= 1:
    #            continue
    #return map(float, vals.split(','))

# Represents one field to be solved: either an image or xylist.
class AstroField(models.Model):
    datasrc_CHOICES = (
        ('url', 'URL'),
        ('file', 'File'),
        )

    filetype_CHOICES = (
        ('image', 'Image (jpeg, png, gif, tiff, or FITS)'),
        ('fits', 'FITS table of source locations'),
        ('text', 'Text list of source locations'),
        )

    user = models.ForeignKey(User, editable=False)

    # Has the user explicitly granted us permission to redistribute this image?
    allowredist = models.BooleanField(default=False)
    # Has the user explicitly forbidden us permission to redistribute this image?
    forbidredist = models.BooleanField(default=False)

    datasrc = models.CharField(max_length=10, choices=datasrc_CHOICES)

    filetype = models.CharField(max_length=10, choices=filetype_CHOICES)

    url = models.URLField(blank=True, null=True)

    uploaded = models.ForeignKey(UploadedFile, null=True, blank=True, editable=False)

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
        s = '<Field'
        #, datasrc %s' , self.datasrc)
        if self.datasrc == 'url':
            s += ', url ' + self.url
        elif self.datasrc == 'file':
            #s += ', file ' + str(self.uploaded)
            #s += ' (originally "%s")' % self.uploaded.userfilename
            s += ', file "%s" (upload id %s)' % (self.uploaded.userfilename, str(self.uploaded))
        s += ', ' + self.filetype
        if self.filetype == 'image' and self.imgtype:
            s += ', ' + self.imgtype
        s += '>'
        return s

    def redistributable(self):
        if self.allowredist:
            return True
        if self.forbidredist:
            return False
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


class Job(models.Model):

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

    status_CHOICES = (
        ('not-submitted', 'Submission failed'),
        ('queued', 'Queued'),
        ('started', 'Started'),
        ('solved', 'Solved'),
        ('failed', 'Failed')
        )

    def __init__(self, *args, **kwargs):
        #log('__init__')
        for k,v in kwargs.items():
            if v is None:
                del kwargs[k]
        kwargs['jobid'] = Job.generate_jobid()
        super(Job, self).__init__(*args, **kwargs)
        self.jobdir = self.get_job_dir()

    jobid = models.CharField(max_length=32, unique=True, editable=False,
                             primary_key=True)

    # has the user explicitly granted anonymous access to this job
    # status page?
    allowanon = models.BooleanField(default=False)

    # has the user explicitly forbidden anonymous access to this job
    # status page?
    forbidanon = models.BooleanField(default=False)

    user = models.ForeignKey(User, editable=False)

    field = models.ForeignKey(AstroField, editable=False)

    parity = models.PositiveSmallIntegerField(choices=parity_CHOICES,
                                              default=2, radio_admin=True,
                                              core=True)

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

    #
    status = models.CharField(max_length=16, choices=status_CHOICES, editable=False)
    failurereason = models.CharField(max_length=256, editable=False)

    # times
    submittime = models.DateTimeField(editable=False, null=True)
    starttime  = models.DateTimeField(editable=False, null=True)
    finishtime = models.DateTimeField(editable=False, null=True)

    # solution
    tanwcs = models.ForeignKey(TanWCS, null=True, editable=False)

    solved = models.BooleanField(default=False)

    ## These fields don't go in the database.

    jobdir = None

    def __str__(self):
        s = '<Job %s, user %s' % (self.jobid, self.user.username)
        if self.status:
            s += ', %s' % self.status
        s += str(self.field) + ' '
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

    def url(self):
        if self.field.datasrc == 'url':
            return self.field.url
        return None

    def userfilename(self):
        if self.field.datasrc == 'file':
            return self.field.uploaded.userfilename
        return None

    def allowanonymous(self):
        if self.allowanon:
            return True
        if self.forbidanon:
            return False
        prefs = UserPreferences.for_user(self.user)
        return prefs.anonjobstatus

    def set_submittime_now(self):
        self.submittime = Job.timenow()
    def set_starttime_now(self):
        self.starttime = Job.timenow()
    def set_finishtime_now(self):
        self.finishtime = Job.timenow()

    def format_submittime(self):
        return Job.format_time(self.submittime)
    def format_starttime(self):
        return Job.format_time(self.starttime)
    def format_finishtime(self):
        return Job.format_time(self.finishtime)

    def format_submittime_brief(self):
        return Job.format_time_brief(self.submittime)

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
            #val = 'between %.2f and %.2f' % (self.scalelower, self.scaleupper)
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

    def get_filename(self, fn):
        return os.path.join(self.get_job_dir(), fn)

    def get_relative_filename(self, fn):
        return os.path.join(self.get_relative_job_dir(), fn)

    def get_job_dir(self):
        return os.path.join(config.jobdir, self.get_relative_job_dir())

    def get_relative_job_dir(self):
        return os.path.join(*self.jobid.split('-'))

    def get_orig_file(self):
        return self.field.filename()

    def create_job_dir(self):
        d = self.get_job_dir()
        # HACK - more careful here...
        if os.path.exists(d):
            return
        mode = 0770
        os.makedirs(d, mode)
        #os.chmod(d, 0770)
        #os.chmod(d, stat.S_IRWXU | stat.S_IRWXG)

    def generate_jobid():
        today = datetime.date.today()
        jobid = '%s-%i%02i-%08i' % (config.siteid, today.year,
                                    today.month, random.randint(0, 99999999))
        return jobid
    generate_jobid = staticmethod(generate_jobid)

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
