import logging
import os.path
import sha

from django.db import models
from django.contrib.auth.models import User
from django.core import validators

import an.gmaps_config as config
from an.portal.log import log

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

    def __str__(self):
        s = ('<UserPreferences: ' + self.user.username +
             ', redistributable: ' + (self.autoredistributable and 'T' or 'F')+
             ', allow anonymous: ' + (self.anonjobstatus and 'T' or 'F') + '>')
        return s

    def for_user(user):
        prefset = UserPreferences.objects.all().filter(user = user)
        if not prefset or not len(prefset):
            # no existing user prefs.
            prefs = UserPreferences(user = user)
        else:
            prefs = prefset[0]
        return prefs
    for_user = staticmethod(for_user)



#from an.portal.job import Job, JobSet, AstroField
#from an.portal.wcs import TanWCS, SipWCS

from an.portal.job import *
from an.portal.wcs import *
