from django.db import models

#import sip

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

