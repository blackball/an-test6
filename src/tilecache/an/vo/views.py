import logging

from django import newforms as forms
from django.db import models
from django.http import HttpResponse, HttpResponseRedirect
from django.newforms import widgets, ValidationError, form_for_model
from django.template import Context, RequestContext, loader

#from an.portal.models import Job, AstroField, UserPreferences

from an import gmaps_config
from an import settings
from an.portal.log import log

def siap(request):
    return HttpResponse('not implemented')

