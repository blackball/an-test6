from django.http import HttpResponse
from django.http import HttpResponseRedirect
from django.template import Context, RequestContext, loader
from django import newforms as forms
from models import UploadedFile
import logging

from an import gmaps_config


logfile = gmaps_config.portal_logfile
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s %(levelname)s %(message)s',
                    filename=logfile,
                    )

class UploadForm(forms.Form):
    upload_id = forms.CharField(max_length=32, widget=forms.HiddenInput)
    file = forms.FileField(widget=forms.FileInput(
        attrs={'size':'40'}))

def uploadform(request):
    if not request.user.is_authenticated():
        return HttpResponse('not authenticated')

    id = UploadedFile.generateId()

    logging.debug("Upload form request.");
    form = UploadForm({'upload_id': id})
    ctxt = {
        'form' : form,
        'action' : '/uploader',
        }
    t = loader.get_template('upload/upload.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def progress_xml(request):
    if not request.user.is_authenticated():
        return HttpResponse('not authenticated')
    if not request.GET:
        return HttpResponse('no GET')
    if not 'upload_id' in request.GET:
        return HttpResponse('no upload_id')
    id = request.GET['upload_id']
    ups = UploadedFile.objects.all().filter(uploadid=id)
    if not len(ups):
        return HttpResponse('no such id')
    up = ups[0]
    tag = up.xml()
    if not len(tag):
        return HttpResponse('no tag')
    res = HttpResponse()
    res['Content-type'] = 'text/xml'
    res.write(tag)
    return res

def progress_ajax(request):
    if not request.user.is_authenticated():
        return HttpResponse('not authenticated')
    if not request.GET:
        return HttpResponse('no GET')
    if not 'upload_id' in request.GET:
        return HttpResponse('no upload_id')
    id = request.GET['upload_id']
    logging.debug("Upload progress request for id %s" % id)

    ctxt = {
        'xmlurl' : ('/upload/xml?upload_id=%s' % id),
        }
    t = loader.get_template('upload/progress-ajax.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

