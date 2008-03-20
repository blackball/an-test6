import urllib
import tempfile
import os
import os.path

from django import newforms as forms
from django.http import HttpResponse, HttpResponseRedirect
from django.newforms import widgets, ValidationError, form_for_model
from django.template import Context, RequestContext, loader
from django.core.urlresolvers import reverse

from an.util.run_command import run_command
from an.util.shell import shell_escape, shell_escape_inside_quotes
import an.gmaps_config as gmaps_config
from an.portal.log import log

def get(request):
    n = int(request.GET.get('n', '1'))
    f = request.GET.get('f', None)
    if f is None:
        t = loader.get_template('portal/faint.html')
        url = reverse(get)
        c = RequestContext(request, {
            'url' : url + '?n=%i' % n + '&f=',
            'next' : url + '?n=%i' % (n+1),
            'prev' : url + '?n=%i' % (n-1),
            })
        return HttpResponse(t.render(c))

    fn = '/home/dstn/faint-motion/%s%03i.png' % (f, n)
    if not os.path.exists(fn):
        return HttpResponse('no such file %s' % fn)
    
    f = open(fn, 'rb')
    res = HttpResponse()
    res['Content-Type'] = 'image/png'
    res.write(f.read())
    return res
