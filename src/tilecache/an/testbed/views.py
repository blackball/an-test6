import django.contrib.auth as auth

from django.db import models
from django.http import HttpResponse, HttpResponseRedirect
from django.newforms import widgets, ValidationError, form_for_model
from django.template import Context, RequestContext, loader

from an.testbed.models import *

def oldjobs(request):
    if not request.user.is_authenticated():
        return HttpResponse("piss off.")

    ojs = OldJob.objects.all()

    ctxt = {
        'oldjobs' : ojs,
        }
    t = loader.get_template('testbed/oldjobs.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

