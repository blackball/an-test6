from django.db import models
from django.contrib.auth.models import User
from django.core import validators

from an.upload.models import UploadedFile

import sha
import re
import time
import random
import os.path
import logging
from datetime import date

import an.gmaps_config as config

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

    scaleunit_CHOICES = (
        ('arcsecperpix', 'arcseconds per pixel'),
        ('arcminwidth' , 'width of the field (in arcminutes)'), 
        ('degreewidth' , 'width of the field (in degrees)'),
        ('focalmm'     , 'focal length of the lens (for 35mm film equivalent sensor)'),
        )

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
        self.origfilename = 'original'

    jobid = models.CharField(max_length=32, unique=True, editable=False,
                             primary_key=True)

    filetype = models.CharField(max_length=10, choices=filetype_CHOICES)

    datasrc = models.CharField(max_length=10, choices=datasrc_CHOICES)

    user = models.ForeignKey(User, editable=False)

    url = models.URLField(blank=True)

    uploaded = models.ForeignKey(UploadedFile, null=True, blank=True, editable=False)
    # filename of the uploaded file on the user's machine.
    #userfilename = models.CharField(max_length=256, editable=False, blank=True)

    # content-type of the uploaded file, if it was compressed
    compressedtype = models.CharField(max_length=64, editable=False, blank=True)
    # content-type of the uploaded file, after compression
    filetype = models.CharField(max_length=64, editable=False)

    # sha-1 hash of the uncompressed file.
    filehash = models.CharField(max_length=40, editable=False)

    # for FITS tables, the names of the X and Y columns.
    xcol = models.CharField(max_length=16, blank=True)
    ycol = models.CharField(max_length=16, blank=True)

    # size of the image
    imagew = models.PositiveIntegerField(editable=False, null=True)
    imageh = models.PositiveIntegerField(editable=False, null=True)

    # scale factor for rendering images; <=1.
    displayscale = models.DecimalField(max_digits=10, decimal_places=10, editable=False, null=True)
    # size to render images.
    displayw = models.PositiveIntegerField(editable=False, null=True)
    displayh = models.PositiveIntegerField(editable=False, null=True)

    parity = models.PositiveSmallIntegerField(choices=parity_CHOICES,
                                              default=2, radio_admin=True,
                                              core=True)

    # image scale.
    scaleunits = models.CharField(max_length=16, choices=scaleunit_CHOICES,
                                  default='degreewidth')
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

    # filename of the original uploaded file
    origfilename = models.CharField(max_length=256, editable=False, blank=True)

    ## These fields don't go in the database.

    jobdir = None

    def __str__(self):
        s = '<Job %s, datasrc %s' % (self.jobid, self.datasrc)
        if self.datasrc == 'url':
            s += ', url ' + self.url
        elif self.datasrc == 'file':
            s += ', file ' + self.uploaded
        s += '>'
        return s

    #def set_jobid(self, jid):
    #    self.jobid = jid
    #    self.jobdir = self.get_job_dir()
    #    self.origfilename = 'original'

    def get_orig_file(self):
        return self.get_job_dir() + '/' + self.origfilename

    def get_job_dir(self):
        d = config.jobdir + '/' + self.jobid.replace('-', '/')
        return d
    
    def create_job_dir(self):
        d = self.get_job_dir()
        os.makedirs(d)

    def generate_jobid():
        today = date.today()
        jobid = '%s-%i%02i-%08i' % (config.siteid, today.year,
                                    today.month, random.randint(0, 99999999))
        return jobid
    generate_jobid = staticmethod(generate_jobid)

#   def __init__(self, vals):
#       super(Job,self).__init__()
#       xysrc = vals['xysrc']
#       self.xysrc = xysrc
#   if xysrc == 'file':
#       ## FIXME
#       pass
#   elif xysrc == 'url':
#       job.url = vals['url']
#   elif xysrc == 'fitsfile':
#   elif xysrc == 'fitsurl':
#       job.url = vals['url']
#   else:
#       raise ValueError, 'unknown xysrc'



