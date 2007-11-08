from django.db import models

import sip

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

    def __init__(self, *args, **kwargs):
        filename = None
        if 'file' in kwargs:
            filename = kwargs['file']
            del kwargs['file']
        super(TanWCS, self).__init__(*args, **kwargs)
        if filename:
            wcs = sip.Tan(kwargs['file'])
            self.crval1 = wcs.crval[0]
            self.crval2 = wcs.crval[1]
            self.crpix1 = wcs.crpix[0]
            self.crpix2 = wcs.crpix[1]
            self.cd11 = wcs.cd[0]
            self.cd12 = wcs.cd[1]
            self.cd21 = wcs.cd[2]
            self.cd22 = wcs.cd[3]
            self.imagew = wcs.imagew
            self.imageh = wcs.imageh

    def to_tanwcs(self):
        tan = sip.Tan()
        #tan.crval = [ self.crval1, self.crval2 ]
        #tan.crpix = [ self.crpix1, self.crpix2 ]
        #tan.cd = [ self.cd11, self.cd12, self.cd21, self.cd22 ]
        tan.crval[0] = self.crval1
        tan.crval[1] = self.crval2
        tan.crpix[0] = self.crpix1
        tan.crpix[1] = self.crpix2
        tan.cd[0] = self.cd11
        tan.cd[1] = self.cd12
        tan.cd[2] = self.cd21
        tan.cd[3] = self.cd22
        tan.imagew = self.imagew
        tan.imageh = self.imageh
        return tan

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

