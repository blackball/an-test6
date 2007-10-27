from django.http import HttpResponse
from django.template import Context, RequestContext, loader
import an.upload.views as uploadviews

def view(request):
    uploadform = '/upload/form/'

    # Inline or IFRAME?
    inline = False
    if 'inline' in request.GET:
        inline = True

    if inline:
        progress_meter_html = uploadviews.get_ajax_html()
        progress_meter_js   = uploadviews.get_ajax_javascript()
        progressform = ''
    else:
        progress_meter_html = ''
        progress_meter_js   = ''
        progressform = '/upload/progress_ajax/?upload_id='

    ctxt = {
        'uploadform' : uploadform,
        'progressform' : progressform,
        'progress_meter_html' : progress_meter_html,
        'progress_meter_js' :progress_meter_js,
        'inline_progress' : inline,
        }

    t = loader.get_template('uploaddemo.html')
    c = Context(ctxt)
    return HttpResponse(t.render(c))
