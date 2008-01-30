from django.db import models

class User(models.Model):
    username = models.CharField(maxlength=255)
    firstactive = models.DateTimeField('first activity date')

class Image(models.Model):
    user = models.ForeignKey(User)
    uri = models.CharField(maxlength=4906)
    discovered = models.DateTimeField('date discovered')

class WatchList(models.Model):
    service = models.CharField(maxlength=255)
    daily_frequency = models.IntegerField()
    images = models.ForeignKey(Image)
    users = models.ForeignKey(User)
    class Admin:
        pass

    
