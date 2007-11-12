from django.db import models

from an.portal.job import AstroField
from an.portal.wcs import TanWCS


class TestbedJob(models.Model):
    field = models.ForeignKey(AstroField)
    wcs = models.ForeignKey(TanWCS)

    origid = models.CharField(max_length=32, null=True)



class OldJob(models.Model):
    jobid = models.CharField(max_length=256, unique=True, primary_key=True)
    jobdir = models.CharField(max_length=256)
    imagefile = models.CharField(max_length=256, null=True)
    email = models.CharField(max_length=64, null=True)
    solved = models.BooleanField()
    imagew = models.PositiveIntegerField()
    imageh = models.PositiveIntegerField()

