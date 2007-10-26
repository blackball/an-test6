from django.db import models
from django.contrib.auth.models import User
from django.core import validators
import sha
import re
import time
import random
import os.path
import logging

from an import gmaps_config


logfile = gmaps_config.portal_logfile
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s %(levelname)s %(message)s',
                    filename=logfile,
                    )


uploadid_re = re.compile('[A-Za-z0-9]{%i}$' % (sha.digest_size*2))
try:
    upload_base_dir = os.environ['UPLOAD_DIR']
except KeyError:
    upload_base_dir = '/tmp'

class UploadedFile(models.Model):
    def isValidId(id):
        return uploadid_re.match(id)
    isValidId = staticmethod(isValidId)

    def generateId():
        h = sha.new()
        h.update(str(time.time()) + str(random.random()))
        uid = h.hexdigest()
        return uid
    generateId = staticmethod(generateId)

    def getFilenameForId(uid):
        return upload_base_dir + '/' + uid
    getFilenameForId = staticmethod(getFilenameForId)

    #def __init__(self, ):

    def __str__(self):
        return self.uploadid

    def validateId(self, field_data, all_data):
        if not self.isValidId(field_data):
            raise validators.ValidationError('Invalid upload ID')

    uploadid = models.CharField(max_length=100,
                                primary_key=True,
                                validator_list=[validateId],
                                )
    user = models.ForeignKey(User, editable=False,
                             blank=True, null=True)

    starttime = models.PositiveIntegerField(default=0)
    nowtime = models.PositiveIntegerField(default=0)
    predictedsize = models.PositiveIntegerField(default=0)
    byteswritten = models.PositiveIntegerField(default=0)
    filesize = models.PositiveIntegerField(default=0)
    errorstring = models.CharField(max_length=256, null=True, default='')

    def xml(self):
        err = self.errorstring
        logging.debug('Error: %s' % err)
        args = {}
        dt = self.nowtime - self.starttime
        args['elapsed'] = dt
        if self.filesize > 0:
            sz = self.filesize
        else:
            sz = self.predictedsize
        args['filesize'] = sz
        if sz:
            args['pct'] = int(round(100 * self.byteswritten / float(sz)))
        if self.byteswritten and dt>0:
            avgspeed = self.byteswritten / float(dt)
            args['speed'] = avgspeed
            left = sz - self.byteswritten
            if left < 0:
                left = 0
            eta = left / avgspeed
            args['eta'] = eta
        if self.errorstring:
            args = {}
            args['error'] = self.errorstring

        tag = '<progress'
        for k,v in args.items():
            tag += ' ' + k + '="' + str(v) + '"'
        tag += ' />'
        return tag

    def get_filename(self):
        return UploadedFile.getFilenameForId(self.uploadid)

    def fileExists(self):
        path = upload_base_dir + '/' + self.uploadid
        return os.path.exists(path)

        


