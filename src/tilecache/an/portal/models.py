import logging
import os.path
import sha

from django.db import models
from django.contrib.auth.models import User
from django.core import validators

import an.gmaps_config as config
from an.portal.log import log

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


from an.portal.job import *
from an.portal.wcs import *
