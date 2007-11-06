import logging

from django import newforms as forms
from django.db import models
from django.http import HttpResponse, HttpResponseRedirect
from django.newforms import widgets, ValidationError, form_for_model
from django.template import Context, RequestContext, loader

#from an.portal.models import Job, AstroField, UserPreferences

from an import gmaps_config
from an import settings
from an.vo.log import log

INT_COVERS = 0
INT_ENCLOSED = 1
INT_CENTER = 2
INT_OVERLAPS = 3

def siap_pointed(request):
    try:
        pos_str = request.GET['POS']
        size_str = request.GET['SIZE']
        pos = map(float, pos_str.split(','))
        size = map(float, size_str.split(','))
        intersect = 
        if 'INTERSECT' in request.GET:
        if len(pos) != 2:
            raise ValueError('POS must contain two values (RA and Dec); got %i' % len(pos))
        if not (len(size) in [1, 2]):
            raise ValueError('SIZE must contain one or two values; got %i' % len(size))
            

        log('POS:', pos, 'SIZE:', size)
        
    except (KeyError, ValueError),e:
        log('key error:', e)
    return HttpResponse('not implemented')

def getimage(request):
    return HttpResponse('not implemented')

