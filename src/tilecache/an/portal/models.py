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

from an.upload.models import UploadedFile



# To install a new database table:
# > python manage.py sql portal
# > sqlite3 /data2/test-django/tile.db
# sqlite> drop table portal_job;
# sqlite>   (paste CREATE TABLE statement)

class Job(models.Model):

    datasrc_CHOICES = (
        ('url', 'URL'),
        ('file', 'File'),
        )

    filetype_CHOICES = (
        ('image', 'Image (jpeg, png, gif, tiff, or FITS)'),
        ('fits', 'FITS table of source locations'),
        ('text', 'Text list of source locations'),
        )

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
        for k,v in kwargs.items():
            if v is None:
                del kwargs[k]
        kwargs['jobid'] = Job.generate_jobid()
        super(Job, self).__init__(*args, **kwargs)
        self.jobdir = self.get_job_dir()

    jobid = models.CharField(max_length=32, unique=True, editable=False,
                             primary_key=True)

    filetype = models.CharField(max_length=10, choices=filetype_CHOICES)

    datasrc = models.CharField(max_length=10, choices=datasrc_CHOICES)

    user = models.ForeignKey(User, editable=False)

    url = models.URLField(blank=True)

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

    # 1 / (scale factor) for rendering images; >=1.
    displayscale = models.DecimalField(max_digits=10, decimal_places=10, editable=False, null=True)
    # size to render images.
    displayw = models.PositiveIntegerField(editable=False, null=True)
    displayh = models.PositiveIntegerField(editable=False, null=True)

    parity = models.PositiveSmallIntegerField(choices=parity_CHOICES,
                                              default=2, radio_admin=True,
                                              core=True)

    # image scale.
    scaleunits = models.CharField(max_length=16, choices=scaleunits_CHOICES,
                                  default=scaleunits_default)
    scaletype  = models.CharField(max_length=3, choices=scaletype_CHOICES,
                                  default='ul')
    scalelower = models.DecimalField(max_digits=20, decimal_places=10,
                                     default=0.1, blank=True, null=True)
    scaleupper = models.DecimalField(max_digits=20, decimal_places=10,
                                     default=180, blank=True, null=True)
    scaleest   = models.DecimalField(max_digits=20, decimal_places=10, blank=True, null=True)
    scaleerr   = models.DecimalField(max_digits=20, decimal_places=10, blank=True, null=True)

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

    solved = models.BooleanField(default=False)

    ## These fields don't go in the database.

    jobdir = None

    def __str__(self):
        s = '<Job %s, user %s' % (self.jobid, self.user.username)
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

    def set_submittime_now(self):
        self.submittime = Job.timenow()
    def set_starttime_now(self):
        self.starttime = Job.timenow()
    def set_finishtime_now(self):
        self.finishtime = Job.timenow()

    def get_scale_bounds(self):
        if self.scaletype == 'ul':
            return (self.scalelower, self.scaleupper)
        elif self.scaletype == 'ev':
            return (self.scaleest * (1 - self.scaleerr / 100.0),
                    self.scaleest * (1 + self.scaleerr / 100.0))
        else:
            return None

    def compute_filehash(self, fn):
        h = sha.new()
        f = open(fn, 'rb')
        while True:
            d = f.read(4096)
            if len(d) == 0:
                break
            h.update(d)
        self.filehash = h.hexdigest()

    def get_filename(self, fn):
        return os.path.join(self.get_job_dir(), fn)

    def get_orig_file(self):
        return self.get_filename('original')

    def get_job_dir(self):
        d = os.path.join(*([config.jobdir,] + self.jobid.split('-')))
        return d
    
    def create_job_dir(self):
        d = self.get_job_dir()
        # HACK - more careful here...
        if os.path.exists(d):
            return
        os.makedirs(d)
        os.chmod(d, stat.S_IRWXU | stat.S_IRWXG)

    def generate_jobid():
        today = datetime.date.today()
        jobid = '%s-%i%02i-%08i' % (config.siteid, today.year,
                                    today.month, random.randint(0, 99999999))
        return jobid
    generate_jobid = staticmethod(generate_jobid)

    def timenow():
        return datetime.datetime.utcnow()
    timenow = staticmethod(timenow)
