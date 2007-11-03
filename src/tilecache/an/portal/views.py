import datetime
import logging
import math
import os.path
import random
import re
import sha
import tempfile
import time

import django.contrib.auth as auth

from django import newforms as forms
from django.db import models
from django.http import HttpResponse, HttpResponseRedirect
from django.newforms import widgets, ValidationError
from django.template import Context, RequestContext, loader

import an.upload as upload
import quads.fits2fits as fits2fits
import quads.image2pnm as image2pnm
import an.portal.mercator as merc

from an.portal.models import Job, AstroField
from an.upload.models import UploadedFile
from an.upload.views  import UploadIdField
from an.portal.convert import convert

from an import gmaps_config
from an import settings
from an.portal.log import log

# Adding a user:
# > python manage.py shell
# >>> from django.contrib.auth.models import User
# >>> email = 'user@domain.com'
# >>> passwd = 'password'
# >>> user = User.objects.create_user(email, email, passwd)
# >>> user.save()

class LoginForm(forms.Form):
    username = forms.EmailField()
    password = forms.CharField(max_length=100, widget=forms.PasswordInput)

ftpurl_re = re.compile(
    r'^ftp://'
    r'(?:(?:[A-Z0-9-]+\.)+[A-Z]{2,6}|' #domain...
    r'localhost|' #localhost...
    r'\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})' # ...or ip
    r'(?::\d+)?' # optional port
    r'(?:/?|/\S+)$', re.IGNORECASE)

class FtpOrHttpURLField(forms.URLField):
    def clean(self, value):
        #log('FtpOrHttpURLField.clean(', value, ')')
        try:
            val = super(FtpOrHttpURLField, self).clean(value)
            return val
        except ValidationError:
            pass
        if ftpurl_re.match(value):
            return value
        raise ValidationError(self.error_message)

class ForgivingURLField(FtpOrHttpURLField):
    def clean(self, value):
        #log('ForgivingURLField.clean(', value, ')')
        if value is not None and \
               (value.startswith('http://http://') or
                value.startswith('http://ftp://')):
            value = value[7:]
        return super(ForgivingURLField, self).clean(value)

class SimpleURLForm(forms.Form):
    url = ForgivingURLField(initial='http://',
                            widget=forms.TextInput(attrs={'size':'50'}))

class SimpleFancyFileForm(forms.Form):
    upload_id = UploadIdField(widget=forms.HiddenInput())

class FullForm(forms.Form):

    datasrc = forms.ChoiceField(choices=AstroField.datasrc_CHOICES,
                                initial='url',
                                widget=forms.RadioSelect(
        attrs={'id':'datasrc',
               'onclick':'datasourceChanged()'}))

    filetype = forms.ChoiceField(choices=AstroField.filetype_CHOICES,
                                 initial='image',
                                 widget=forms.Select(
        attrs={'onchange':'filetypeChanged()',
               'onkeyup':'filetypeChanged()'}))

    upload_id = UploadIdField(widget=forms.HiddenInput(),
                              required=False)

    scaleunits = forms.ChoiceField(choices=Job.scaleunits_CHOICES,
                                   initial=Job.scaleunits_default,
                                   widget=forms.Select(
        attrs={'onchange':'unitsChanged()',
               'onkeyup':'unitsChanged()'}))
    scaletype = forms.ChoiceField(choices=Job.scaletype_CHOICES,
                                  widget=forms.RadioSelect(
        attrs={'id':'scaletype',
               'onclick':'scalechanged()'}),
                                  initial='ul')
    parity = forms.ChoiceField(choices=Job.parity_CHOICES,
                               initial=2)
    scalelower = forms.DecimalField(widget=forms.TextInput(
        attrs={'onfocus':'setFsUl()',
               'onkeyup':'scalechanged()',
               'size':'5',
               }), initial=0.1, required=False, min_value=0)
    scaleupper = forms.DecimalField(widget=forms.TextInput(
        attrs={'onfocus':'setFsUl()',
               'onkeyup':'scalechanged()',
               'size':'5',
               }), initial=180, required=False, min_value=0)
    scaleest = forms.DecimalField(widget=forms.TextInput(
        attrs={'onfocus':'setFsEv()',
               'onkeyup':'scalechanged()',
               'size':'5',
               }), required=False, min_value=0)
    scaleerr = forms.DecimalField(widget=forms.TextInput(
        attrs={'onfocus':'setFsEv()',
               'onkeyup':'scalechanged()',
               'size':'5',
               }), required=False, min_value=0, max_value=100)
    file = forms.FileField(widget=forms.FileInput(
        attrs={'size':'40',
               'onfocus':'setDatasourceFile()',
               'onclick':'setDatasourceFile()',
               }),
                           required=False)
    url = ForgivingURLField(initial='http://',
                            widget=forms.TextInput(
        attrs={'size':'50',
               'onfocus':'setDatasourceUrl()',
               'onclick':'setDatasourceUrl()',
               }),
                            required=False)
    xcol = forms.CharField(initial='X',
                           required=False,
                           widget=forms.TextInput(attrs={'size':'10'}))
    ycol = forms.CharField(initial='Y',
                           required=False,
                           widget=forms.TextInput(attrs={'size':'10'}))

    tweak = forms.BooleanField(initial=True, required=False)

    tweakorder = forms.IntegerField(initial=2, min_value=2,
                                    max_value=10,
                                    widget=forms.TextInput(attrs={'size':'5'}))

    # How to clean an individual field:
    #def clean_scaletype(self):
    #   val = self.getclean('scaletype')
    #   return val

    def getclean(self, name):
        if not hasattr(self, 'cleaned_data'):
            return None
        if name in self.cleaned_data:
            return self.cleaned_data[name]
        return None

    def geterror(self, name):
        if name in self._errors:
            return self._errors[name]
        return None

    def reclean(self, name):
        field = self.fields[name].field
        value = field.widget.value_from_datadict(self.data, self.files, self.add_prefix(name))
        try:
            value = field.clean(value)
            self.cleaned_data[name] = value
            if hasattr(self, 'clean_%s' % name):
                value = getattr(self, 'clean_%s' % name)()
                self.cleaned_data[name] = value
        except ValidationError, e:
            self._errors[name] = e.messages
            if name in self.cleaned_data:
                del self.cleaned_data[name]             

    def clean(self):
        """Take a shower"""
        src = self.getclean('datasrc')
        if src == 'url':
            if self.geterror('file'):
                del self._errors['file']
            if self.geterror('upload_id'):
                del self._errors['upload_id']
            url = self.getclean('url')
            log('url is ' + str(url))
            if not self.geterror('url'):
                if not (url and len(url)):
                    self._errors['url'] = ['URL is required']
                elif not (url.startswith('http://') or url.startswith('ftp://')):
                    self._errors['url'] = ['Only http and ftp URLs are supported']

        elif src == 'file':
            if self.geterror('url'):
                del self._errors['url']
            fil = self.getclean('file')
            uploadid = self.getclean('upload_id')
            # FIXME - umm... little more checking here :)
            log('file is ' + str(fil) + ', upload id is ' + str(uploadid))
            if not (fil or uploadid):
                self._errors['file'] = ['You must upload a file']

        filetype = self.getclean('filetype')
        if filetype == 'fits':
            xcol = self.getclean('xcol')
            ycol = self.getclean('ycol')
            fitscol = re.compile('^[\w_-]+$')
            if not self.geterror('xcol'):
                if not xcol or len(xcol) == 0:
                    self._errors['xcol'] = ['You must specify the X column name.']
                elif not fitscol.match(xcol):
                    self._errors['xcol'] = ['X column name must be alphanumeric.']
            if not self.geterror('ycol'):
                if not ycol or len(ycol) == 0:
                    self._errors['ycol'] = ['You must specify the Y column name.']
                elif not fitscol.match(ycol):
                    self._errors['ycol'] = ['Y column name must be alphanumeric.']

        if self.getclean('tweak'):
            order = self.getclean('tweakorder')
            print 'tweak order', order
            if not order:
                self._errors['tweakorder'] = ['Tweak order is required if tweak is enabled.']

        log('form clean(): scaletype is %s' % self.getclean('scaletype'))
        scaletype = self.getclean('scaletype')
        if scaletype == 'ul':
            if not self.geterror('scalelower') and not self.geterror('scaleupper'):
                lower = self.getclean('scalelower')
                upper = self.getclean('scaleupper')
                if lower is None:
                    self._errors['scalelower'] = ['You must enter a lower bound.']
                if upper is None:
                    self._errors['scaleupper'] = ['You must enter an upper bound.']
                if (lower > upper):
                    self._errors['scalelower'] = ['Lower bound must be less than upper bound.']
        elif scaletype == 'ev':
            if not self.geterror('scaleest') and not self.geterror('scaleerr'):
                est = self.getclean('scaleest')
                err = self.getclean('scaleerr')
                if est is None:
                    self._errors['scaleest'] = ['You must enter a scale estimate.']
                if err is None:
                    self._errors['scaleerr'] = ['You must enter a scale error bound.']

        # If tweak is off, ignore errors in tweakorder.
        if self.getclean('tweak') == False:
            if self.geterror('tweakorder'):
                del self._errors['tweakorder']
                self.cleaned_data['tweakorder'] = 0

        return self.cleaned_data


def login(request, redirect_to=None):
    form = LoginForm(request.POST)
    authfailed = False
    usererr = None
    passerr = None
    if form.is_valid():
        username = form.cleaned_data['username']
        password = form.cleaned_data['password']

        user = auth.authenticate(username=username, password=password)
        if user is not None:
            if user.is_active:
                auth.login(request, user)
                # Success
                return HttpResponseRedirect('/job/newurl')
            else:
                authfailed = True
                passerr = 'Your account is not active.'
                #<br />Please contact ' + settings.ACCOUNT_ADMIN + ' for help.'
        else:
            # Failure
            authfailed = True
            passerr = 'Incorrect password.'
            #<br />Please contact ' + settings.ACCOUNT_ADMIN + ' if you have forgotten your password.'
            
    else:
        if len(request.POST):
            usererr = len(form['username'].errors) and form['username'].errors[0] or None
            passerr = len(form['password'].errors) and form['password'].errors[0] or None

    t = loader.get_template('portal/login.html')
    c = Context({
        'form' : form,
        'usererr' : usererr,
        'passerr' : passerr,
        })
    return HttpResponse(t.render(c))

def logout(request):
    if not request.user.is_authenticated():
        return HttpResponse("piss off.")
    auth.logout(request)
    return HttpResponseRedirect('/login')

def newurl(request):
    if not request.user.is_authenticated():
        return HttpResponseRedirect('/login')
    urlerr = None
    if len(request.POST):
        form = SimpleURLForm(request.POST)
        if form.is_valid():
            url = form.cleaned_data['url']
            field = AstroField(user = request.user,
                               filetype = 'image',
                               datasrc = 'url',
                               url = url)
            field.save()
            job = Job(user = request.user,
                      field = field)
            submit_job(request, job)
            return HttpResponseRedirect(get_status_url(job))
        else:
            urlerr = form['url'].errors[0]
    else:
        if 'jobid' in request.session:
            del request.session['jobid']
        form = SimpleURLForm()
        
    t = loader.get_template('portal/newjoburl.html')
    c = RequestContext(request, {
        'form' : form,
        'urlerr' : urlerr,
        })
    return HttpResponse(t.render(c))

def get_status_url(job):
    return '/job/status/?jobid=' + job.jobid

def newfile(request):
    if not request.user.is_authenticated():
        return HttpResponseRedirect('/login')

    log('newfile()')
    printvals(request)

    if len(request.POST):
        form = SimpleFancyFileForm(request.POST)
        if form.is_valid():
            field = AstroField(user = request.user,
                               filetype = 'image',
                               datasrc = 'file',
                               uploaded = form.cleaned_data['upload_id'],
                               )
            field.save()
            job = Job(user = request.user,
                      field = field,
                      )
            log('newfile: submitting job ' + str(job))
            submit_job(request, job)
            return HttpResponseRedirect(get_status_url(job))
        else:
            log('form not valid.')
    else:
        if 'jobid' in request.session:
            del request.session['jobid']
        form = SimpleFancyFileForm()
        
    t = loader.get_template('portal/newjobfile.html')
    c = RequestContext(request, {
        'form' : form,
        'uploadform' : '/upload/form/',
        'progressform' : '/upload/progress_ajax/?upload_id='
        })
    return HttpResponse(t.render(c))

def submit_job(request, job):
    log('submit(): Job is: ' + str(job))
    os.umask(07)
    job.create_job_dir()
    job.set_submittime_now()
    job.status = 'Queued'
    job.save()
    request.session['jobid'] = job.jobid

    # enqueue the axy file.
    jobdir = job.get_job_dir()
    link = gmaps_config.jobqueuedir + job.jobid
    os.symlink(jobdir, link)

def get_job(jobid):
    jobset = Job.objects.all().filter(jobid=jobid)
    if len(jobset) != 1:
        log('Found %i jobs, not 1' % len(jobset))
    job = jobset[0]
    return job

def get_url(job, fn):
    return '/job/getfile?jobid=%s&f=%s' % (job.jobid, fn)

def getsessionjob(request):
    if not 'jobid' in request.session:
        log('no jobid in session')
        return None
    jobid = request.session['jobid']
    return get_job(jobid)

#def getgetjob(request):
#    if not request.GET:
#        return HttpResponse('no GET')
#    if not 'jobid' in request.GET:
#        return HttpResponse('no jobid')
#    jobid = request.GET['jobid']
#    job = getjobbyid(jobid)
#    if not job:
#        return HttpResponse('no such job')

def jobstatus(request):
    #if not request.user.is_authenticated():
    #    return HttpResponseRedirect('/login')
    #job = getsessionjob(request)
    #if not job:
    #    return HttpResponse('no job in session')

    if not request.GET:
        return HttpResponse('no GET')
    if not 'jobid' in request.GET:
        return HttpResponse('no jobid')
    jobid = request.GET['jobid']
    job = get_job(jobid)
    if not job:
        return HttpResponse('no such job')

    field = job.field

    log('jobstatus: Job is: ' + str(job))

    ctxt = {
        'jobid' : job.jobid,
        'jobstatus' : job.status,
        'jobsolved' : job.solved,
        'jobsubmittime' : job.format_submittime(),
        'jobstarttime' : job.format_starttime(),
        'jobfinishtime' : job.format_finishtime(),
        'logurl' : get_url(job, 'blind.log'),
        'job' : job,
        'joburl' : (field.datasrc == 'url') and field.url or None,
        'jobfile' : (field.datasrc == 'file') and field.uploaded.userfilename or None,
        'jobscale' : job.friendly_scale(),
        'jobparity' : job.friendly_parity(),
        'sources' : get_url(job, 'sources'),
        'sources_big' : get_url(job, 'sources-big'),
        }



    if job.solved:
        wcsinfofn = convert(job, 'wcsinfo', store_imgtype=True, store_imgsize=True)
        f = open(wcsinfofn)
        wcsinfotxt = f.read()
        f.close()
        wcsinfo = {}
        for ln in wcsinfotxt.split('\n'):
            s = ln.split(' ')
            if len(s) == 2:
                wcsinfo[s[0]] = s[1]

        ctxt.update({'racenter' : '%.2f' % float(wcsinfo['ra_center']),
                     'deccenter': '%.2f' % float(wcsinfo['dec_center']),
                     'fieldw'   : '%.2f' % float(wcsinfo['fieldw']),
                     'fieldh'   : '%.2f' % float(wcsinfo['fieldh']),
                     'fieldunits': wcsinfo['fieldunits'],
                     'racenter_hms' : wcsinfo['ra_center_hms'],
                     'deccenter_dms' : wcsinfo['dec_center_dms'],
                     'orientation' : '%.3f' % float(wcsinfo['orientation']),
                     'pixscale' : '%.4g' % float(wcsinfo['pixscale']),
                     'parity' : (float(wcsinfo['det']) > 0 and 'Positive' or 'Negative'),
                     'wcsurl' : get_url(job, 'wcs.fits'),
                     })



        objsfn = convert(job, 'objsinfield', store_imgtype=True, store_imgsize=True)
        f = open(objsfn)
        objtxt = f.read()
        f.close()
        objs = objtxt.strip()
        if len(objs):
            objs = objs.split('\n')
        else:
            objs = []
        ctxt['objsinfield'] = objs

        # deg
        fldsz = math.sqrt(field.imagew * field.imageh) * float(wcsinfo['pixscale']) / 3600.0

        url = (gmaps_config.tileurl + '?layers=tycho,grid,userboundary' +
               '&arcsinh&wcsfn=%s' % job.get_relative_filename('wcs.fits'))
        smallstyle = '&w=300&h=300&lw=3'
        largestyle = '&w=1024&h=1024&lw=5'
        steps = [ {              'gain':-0.5,  'dashbox':0.1,   'center':False },
                  {'limit':18,   'gain':-0.25, 'dashbox':0.01,  'center':True, 'dm':0.05 },
                  {'limit':1.8,  'gain':0.5,                    'center':True, 'dm':0.005 },
                  ]

        zlist = []
        for s in steps:
            if 'limit' in s and fldsz > s['limit']:
                break
            if s['center']:
                xmerc = float(wcsinfo['ra_center_merc'])
                ymerc = float(wcsinfo['dec_center_merc'])
                ralo = merc.merc2ra(xmerc + s['dm'])
                rahi = merc.merc2ra(xmerc - s['dm'])
                declo = merc.merc2dec(ymerc - s['dm'])
                dechi = merc.merc2dec(ymerc + s['dm'])
                bb = [ralo, declo, rahi, dechi]
            else:
                bb = [0, -85, 360, 85]
            urlargs = ('&gain=%g' % s['gain']) + ('&bb=' + ','.join(map(str, bb)))
            if 'dashbox' in s:
                urlargs += '&dashbox=%g' % s['dashbox']

            zlist.append([url + smallstyle + urlargs,
                          url + largestyle + urlargs])

        # HACK
        fn = convert(job, 'fullsizepng')
        url = (gmaps_config.gmaps_url +
               ('?zoom=%i&ra=%.3f&dec=%.3f&userimage=%s' %
                (int(wcsinfo['merczoom']), float(wcsinfo['ra_center']),
                 float(wcsinfo['dec_center']), job.get_relative_job_dir())))

        ctxt.update({
            'gmapslink' : url,
            'zoomimgs'  : zlist,
            'annotation': get_url(job, 'annotation'),
            'annotation_big' : get_url(job, 'annotation-big'),
            })

    else:
        logfn = job.get_filename('blind.log')
        if os.path.exists(logfn):
            f = open(logfn)
            logfiletxt = f.read()
            f.close()
            lines = logfiletxt.split('\n')
            lines = '\n'.join(lines[-16:])
            log('job not solved')

            ctxt.update({
                'logfile_tail' : lines,
                })

    t = loader.get_template('portal/status.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def file_size(fn):
    st = os.stat(fn)
    return st.st_size

def getfile(request):
    #if not request.user.is_authenticated():
    #    return HttpResponseRedirect('/login')
    #job = getsessionjob(request)
    #if not job:
    #    return HttpResponse('no job in session')
    #if not request.GET:
    #    return HttpResponse('no GET')

    if not request.GET:
        return HttpResponse('no GET')
    if not 'jobid' in request.GET:
        return HttpResponse('no jobid')
    jobid = request.GET['jobid']
    job = get_job(jobid)
    if not job:
        return HttpResponse('no such job')

    if not 'f' in request.GET:
        return HttpResponse('no f=')

    f = request.GET['f']

    pngimages = [ 'annotation', 'annotation-big',
                  'sources', 'sources-big' ]

    res = HttpResponse()
    if f in pngimages:
        fn = convert(job, f)
        res['Content-Type'] = 'image/png'
        res['Content-Length'] = file_size(fn)
        f = open(fn)
        res.write(f.read())
        f.close()
        return res

    binaryfiles = [ 'wcs.fits', 'match.fits' ]
    if f in binaryfiles:
        fn = job.get_filename(f)
        res['Content-Type'] = 'application/octet-stream'
        res['Content-Disposition'] = 'attachment; filename="' + f + '"'
        res['Content-Length'] = file_size(fn)
        f = open(fn)
        res.write(f.read())
        f.close()
        return res

    textfiles = [ 'blind.log' ]
    if f in textfiles:
        fn = job.get_filename(f)
        res['Content-Type'] = 'text/plain'
        res['Content-Disposition'] = 'inline'
        res['Content-Length'] = file_size(fn)
        f = open(fn)
        res.write(f.read())
        f.close()
        return res

    return HttpResponse('bad f')

def printvals(request):
    if request.POST:
        log('POST values:')
        for k,v in request.POST.items():
            log('  %s = %s' % (str(k), str(v)))
    if request.GET:
        log('GET values:')
        for k,v in request.GET.items():
            log('  %s = %s' % (str(k), str(v)))
    if request.FILES:
        log('FILES values:')
        for k,v in request.FILES.items():
            log('  %s = %s' % (str(k), str(v)))

# Note, if there are *ANY* errors in the form, it will have no
# 'cleaned_data' array.

def newlong(request):
    if not request.user.is_authenticated():
        return HttpResponseRedirect('/login')

    if request.POST:
        log('POST values:')
        for k,v in request.POST.items():
            log('  %s = %s' % (str(k), str(v)))
        form = FullForm(request.POST, request.FILES)
    else:
        form = FullForm()

    if form.is_valid():
        print 'Yay'

        uploaded = form.getclean('upload_id')
        form.getclean('url')
        if form.getclean('filetype') == 'url':
            uploaded = None
        elif form.getclean('filetype') == 'file':
            url = None

        field = AstroField(
            user = request.user,
            datasrc = form.getclean('datasrc'),
            filetype = form.getclean('filetype'),
            uploaded = uploaded,
            url = url,
            xcol = form.getclean('xcol'),
            ycol = form.getclean('ycol'),
            )
        field.save()

        job = Job(user = request.user,
                  field = field,
                  parity = form.getclean('parity'),
                  scaleunits = form.getclean('scaleunits'),
                  scaletype = form.getclean('scaletype'),
                  scalelower = form.getclean('scalelower'),
                  scaleupper = form.getclean('scaleupper'),
                  scaleest = form.getclean('scaleest'),
                  scaleerr = form.getclean('scaleerr'),
                  tweak = form.getclean('tweak'),
                  tweakorder = form.getclean('tweakorder'),
                  )

        log('Form is valid.')
        for k,v in form.cleaned_data.items():
            log('  %s = %s' % (str(k), str(v)))

        log('Job: ' + str(job))

        submit_job(request, job)
        return HttpResponseRedirect(get_status_url(job))

    if 'jobid' in request.session:
        del request.session['jobid']

    log('Errors:')
    if form._errors:
        for k,v in form._errors.items():
            log('  %s = %s' % (str(k), str(v)))

    # Ugh, special rendering for radio checkboxes....
    scaletype = form['scaletype'].field
    widg = scaletype.widget
    attrs = widg.attrs
    if request.POST:
        val = widg.value_from_datadict(request.POST, None, 'scaletype')
    else:
        val = form.getclean('scaletype') or scaletype.initial
    render = widg.get_renderer('scaletype', val, attrs)
    r0 = render[0]
    r1 = render[1]
    r0txt = r0.tag()
    r1txt = r1.tag()

    datasrc = form['datasrc'].field
    widg = datasrc.widget
    attrs = widg.attrs
    if request.POST:
        val = widg.value_from_datadict(request.POST, None, 'datasrc')
    else:
        val = form.getclean('datasrc') or datasrc.initial
    render = widg.get_renderer('datasrc', val, attrs)
    ds0 = render[0].tag()
    ds1 = render[1].tag()

    uploadform = '/upload/form/'

    inline = False

    if inline:
        progress_meter_html = upload.views.get_ajax_html()
        progress_meter_js   = upload.views.get_ajax_javascript()
        progressform = ''
    else:
        progress_meter_html = ''
        progress_meter_js   = ''
        progressform = '/upload/progress_ajax/?upload_id='

    ctxt = {
        'form' : form,
        'uploadform' : uploadform,
        'progressform' : progressform,
        'myurl' : '/job/newlong/',
        'scale_ul' : r0txt,
        'scale_ee' : r1txt,
        'datasrc_url' : ds0,
        'datasrc_file' : ds1,
        'progress_meter_html' : progress_meter_html,
        'progress_meter_js' :progress_meter_js,
        'inline_progress' : inline,
        }

    errfields = [ 'scalelower', 'scaleupper', 'scaleest', 'scaleerr',
                  'tweakorder', 'xcol', 'ycol', 'scaletype', 'datasrc',
                  'url' ]
    for f in errfields:
        ctxt[f + '_err'] = len(form[f].errors) and form[f].errors[0] or None

    if request.POST:
        if request.FILES:
            f1 = request.FILES['file']
            print 'file is', repr(f1)
        else:
            print 'no file uploaded'

    t = loader.get_template('portal/newjoblong.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))
