from an.portal.models import Job
from django import newforms as forms
from django.http import HttpResponse
from django.http import HttpResponseRedirect
from django.template import Context, RequestContext, loader
import django.contrib.auth as auth
from django.newforms import widgets
import re
import time
import random
from an import settings
#import sys
import logging
from an import gmaps_config
import sha

# Adding a user:
# > python manage.py shell
# >>> from django.contrib.auth.models import User
# >>> email = 'user@domain.com'
# >>> passwd = 'password'
# >>> user = User.objects.create_user(email, email, passwd)
# >>> user.save()

logfile = gmaps_config.portal_logfile
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s %(levelname)s %(message)s',
                    filename=logfile,
                    )

class LoginForm(forms.Form):
    username = forms.EmailField()
    password = forms.CharField(max_length=100, widget=forms.PasswordInput)

class ForgivingURLField(forms.URLField):
    def clean(self, value):
        print 'cleaning value', value
        if value is not None and \
               (value.startswith('http://http://') or
                value.startswith('http://ftp://')):
            value = value[7:]
        return super(ForgivingURLField, self).clean(value)

class SimpleURLForm(forms.Form):
    url = ForgivingURLField(initial='http://',
                            widget=forms.TextInput(attrs={'size':'50'}))

class SimpleFileForm(forms.Form):
    file = forms.FileField(widget=forms.FileInput(attrs={'size':'40'}))

class FullForm(forms.Form):

    datasrc_CHOICES = (
        ('url', 'URL'),
        ('file', 'File'),
        )

    filetype_CHOICES = (
        ('image', 'Image (jpeg, png, gif, tiff, or FITS)'),
        ('fits', 'FITS table of source locations'),
        ('text', 'Text list of source locations'),
        )

    datasrc = forms.ChoiceField(choices=datasrc_CHOICES,
                                initial='url',
                                widget=forms.RadioSelect(
        attrs={'id':'datasrc',
               'onclick':'datasourceChanged()'}),
                                # HACK
                                required=False
                                )

    filetype = forms.ChoiceField(choices=filetype_CHOICES,
                                 initial='image',
                                 widget=forms.Select(
        attrs={'onchange':'filetypeChanged()',
               'onkeyup':'filetypeChanged()'}),
                                 # HACK
                                 required=False
                                 )

    upload_id = forms.CharField(widget=forms.HiddenInput(),
                                required=False)

    xysrc = forms.ChoiceField(choices=Job.xysrc_CHOICES,
                              initial='url',
                              widget=forms.Select(
        attrs={'onchange':'sourceChanged()',
               'onkeyup':'sourceChanged()'}),
                              # HACK
                              required=False
                              )
    scaleunits = forms.ChoiceField(choices=Job.scaleunit_CHOICES,
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
        xysrc = self.getclean('xysrc')
        print 'clean() called; xysrc is', xysrc
        if (xysrc == 'url') or (xysrc == 'fitsurl'):
            urlerr = self.geterror('url')
            urlval = self.getclean('url')
            print 'url error:', urlerr
            print 'url value:', urlval
            if urlerr is None:
                if urlval is None or (len(urlval) == 0):
                    self._errors['url'] = ['URL is required']
        elif (xysrc == 'file') or (xysrc == 'fitsfile'):
            if not self.geterror('file'):
                file = self.getclean('file')
                print 'file is',repr(file)
                print 'file in self.files is',repr('file' in self.files and self.files['file'] or None)
                if not file:
                    self._errors['file'] = ['You must upload a file.']

        if (xysrc == 'fitsfile') or (xysrc == 'fitsurl'):
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

        print 'form clean(): scaletype is', self.getclean('scaletype')
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
    logging.debug('login()')
    if form.is_valid():
        username = form.cleaned_data['username']
        password = form.cleaned_data['password']

        logging.debug('calling auth.authenticate()...')
        user = auth.authenticate(username=username, password=password)
        logging.debug('auth.authenticate() returned.')
        if user is not None:
            if user.is_active:
                logging.debug('calling auth.login().')
                auth.login(request, user)
                logging.debug('auth.login() returned.')
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
    logging.debug('calling user.is_authenticated()...')
    ok = request.user.is_authenticated()
    logging.debug('user.is_authenticated() returned', ok)
    #if not request.user.is_authenticated():
    if not ok:
        return HttpResponseRedirect('/login')
    urlerr = None
    if len(request.POST):
        form = SimpleURLForm(request.POST)
        if form.is_valid():
            url = form.cleaned_data['url']
            job = Job()
            job.user = user
            job.xysrc = 'url'
            request.session['job'] = job
            return HttpResponseRedirect('/job/submit')
        else:
            urlerr = form['url'].errors[0]
    else:
        request.session['job'] = None
        form = SimpleURLForm()
        
    t = loader.get_template('portal/newjobsimple.html')
    c = RequestContext(request, {
        'form' : form,
        'isurl' : True,
        'urlerr' : urlerr,
        'fileerr' : None,
        })
    return HttpResponse(t.render(c))

def newfile(request):
    if not request.user.is_authenticated():
        return HttpResponseRedirect('/login')
    fileerr = None
    if len(request.POST):
        form = SimpleFileForm(request.POST, request.FILES)
        if form.is_valid():
            job = Job()
            job.user = user
            job.xysrc = 'file'
            request.session['job'] = job
            return HttpResponseRedirect('/job/submit')
        else:
            fileerr = form['file'].errors[0]
    else:
        request.session['job'] = None
        form = SimpleFileForm()
        
    t = loader.get_template('portal/newjobsimple.html')
    c = RequestContext(request, {
        'form' : form,
        'isurl' : False,
        'urlerr' : None,
        'fileerr' : fileerr,
        })
    return HttpResponse(t.render(c))

def newlong(request):
    if not request.user.is_authenticated():
        return HttpResponseRedirect('/login')

    if request.POST:
        logging.debug('POST values:')
        for k,v in request.POST.items():
            logging.debug('  %s = %s' % (str(k), str(v)))
        form = FullForm(request.POST, request.FILES)
    else:
        form = FullForm()

    #form.full_clean()
    if form.is_valid():
        print 'Yay'
        request.session['jobvals'] = form.cleaned_data
        return HttpResponseRedirect('/job/submit')

    if 'jobvals' in request.session:
        del request.session['jobvals']

    # Note, if there are *ANY* errors, the form will have no 'cleaned_data'
    # array.

    logging.debug('Errors:')
    if form._errors:
        for k,v in form._errors.items():
            logging.debug('  %s = %s' % (str(k), str(v)))

    #print 'scaletype is', form.getclean('scaletype')
    #scaletype = form['scaletype'].field
    #widg = scaletype.widget
    #val = widg.value_from_datadict(request.POST, request.FILES, 'scaletype')
    #print 'scaletype val is', val

    # Ugh, special rendering for radio checkboxes....
    scaletype = form['scaletype'].field
    widg = scaletype.widget
    attrs = widg.attrs
    if request.POST:
        val = widg.value_from_datadict(request.POST, None, 'scaletype')
    else:
        val = form.getclean('scaletype') or scaletype.initial
    render = widg.get_renderer('scaletype', val, attrs)
    #print 'scaletype val is', val
    r0 = render[0]
    r1 = render[1]
    r0txt = r0.tag()
    r1txt = r1.tag()
    #print 'checkbox 0:', r0txt
    #print 'checkbox 1:', r1txt

    ctxt = {
        'form' : form,
        'scale_ul' : r0txt,
        'scale_ee' : r1txt,
        }
    errfields = [ 'scalelower', 'scaleupper', 'scaleest', 'scaleerr',
                  'tweakorder', 'xcol', 'ycol', 'scaletype' ]
    for f in errfields:
        ctxt[f + '_err'] = len(form[f].errors) and form[f].errors[0] or None

    if request.POST:
        #form.is_valid()

        if request.FILES:
            f1 = request.FILES['file']
            print 'file is', repr(f1)
        else:
            print 'no file uploaded'

        urlerr  = len(form['url'].errors)  and form['url'].errors [0] or None
        fileerr = len(form['file'].errors) and form['file'].errors[0] or None
        print 'url error is', urlerr
        print 'file error is', fileerr
        val = form['xysrc'].field.clean(request.POST['xysrc'])
        if val == 'url':
            ctxt['imgurl_err'] = urlerr
        elif val == 'file':
            ctxt['imgfile_err'] = fileerr
        elif val == 'fitsurl':
            ctxt['fitsurl_err'] = urlerr
        elif val == 'fitsfile':
            ctxt['fitsfile_err'] = fileerr

    t = loader.get_template('portal/newjoblong.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def submit(request):
    if not request.user.is_authenticated():
        return HttpResponseRedirect('/login')

    txt = "<pre>Job values:\n"
    for k,v in request.session['jobvals'].items():
        txt += '  ' + str(k) + ' = ' + str(v) + '\n'
    txt += "</pre>"

    logging.debug('Job values:')
    for k,v in request.session['jobvals'].items():
        logging.debug('  %s = %s' % (str(k), str(v)))

    return HttpResponse(txt)



class UploadForm(forms.Form):
    upload_id = forms.CharField(max_length=32, widget=forms.HiddenInput)
    file = forms.FileField(widget=forms.FileInput(
        attrs={'size':'40'}))


def printvals(request):
    if request.POST:
        logging.debug('POST values:')
        for k,v in request.POST.items():
            logging.debug('  %s = %s' % (str(k), str(v)))
    if request.GET:
        logging.debug('GET values:')
        for k,v in request.GET.items():
            logging.debug('  %s = %s' % (str(k), str(v)))
    if request.FILES:
        logging.debug('FILES values:')
        for k,v in request.FILES.items():
            logging.debug('  %s = %s' % (str(k), str(v)))


def upload(request):
    if not request.user.is_authenticated():
        return HttpResponse('not authenticated')

    logging.debug("Upload request.");

    if request.GET:
        form = UploadForm(request.GET)
    elif request.POST:
        form = UploadForm(request.POST, request.FILES)
    else:
        form = UploadForm()

    if request.POST:
        logging.debug('POST values:')
        for k,v in request.POST.items():
            logging.debug('  %s = %s' % (str(k), str(v)))
    if request.GET:
        logging.debug('GET values:')
        for k,v in request.GET.items():
            logging.debug('  %s = %s' % (str(k), str(v)))
    if request.FILES:
        logging.debug('FILES values:')
        for k,v in request.FILES.items():
            logging.debug('  %s = %s' % (str(k), str(v)))


    if form.is_valid():
        sz = len(form.cleaned_data['file'].content)
        logging.debug("Successful upload: file size %d" % sz)
        return HttpResponse("file uploaded: size %d" % sz)
    else:
        logging.debug('Invalid form: errors:')
        for k,v in form.errors.items():
            logging.debug('  %s = %s' % (str(k), str(v)))

    ctxt = {
        'form' : form,
        }

    t = loader.get_template('portal/upload.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def uploadform(request):
    if not request.user.is_authenticated():
        return HttpResponse('not authenticated')

    id = str(time.time()) + str(random.random())
    h = sha.new()
    h.update(id)
    id = h.hexdigest()

    logging.debug("Upload form request.");
    form = UploadForm({'upload_id': id})
    ctxt = {
        'form' : form,
        }
    t = loader.get_template('portal/upload.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def uploadprogress(request):
    if not request.user.is_authenticated():
        return HttpResponse('not authenticated')
    if not request.GET:
        return HttpResponse('no GET')
    if not 'upload_id' in request.GET:
        return HttpResponse('no upload_id')
    id = request.GET['upload_id']
    logging.debug("Upload progress request for id %s" % id)
    printvals(request)

    if 'xml' in request.GET:
        # HACK - this path should be a config option!
        f = open('/tmp/%s.progress' % id)
        if not f:
            return HttpResponse('no such id')
        tag = f.read()
        f.close()
        if not len(tag):
            return HttpResponse('no tag')
        res = HttpResponse()
        res['Content-type'] = 'text/xml'
        res.write(tag)
        return res
    
    ctxt = {
        'refresh' : ('5; URL=/job/uploadprogress?upload_id=%s' % id),
        'time_sofar' : '0:10',
        'time_remaining' : '0:30',
        'bytes_sofar' : '100 k',
        'bytes_total' : '500 k',
        'showstats' : False,

        'pct' : 0,

        'xmlurl' : ('/job/uploadprogress?xml&upload_id=%s' % id),
        }
    t = loader.get_template('portal/uploadprogress2.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def newlong2(request):
    if not request.user.is_authenticated():
        return HttpResponseRedirect('/login')

    if request.POST:
        logging.debug('POST values:')
        for k,v in request.POST.items():
            logging.debug('  %s = %s' % (str(k), str(v)))
        form = FullForm(request.POST, request.FILES)
    else:
        form = FullForm()

    if form.is_valid():
        print 'Yay'
        request.session['jobvals'] = form.cleaned_data
        return HttpResponseRedirect('/job/submit')

    if 'jobvals' in request.session:
        del request.session['jobvals']

    # Note, if there are *ANY* errors, the form will have no 'cleaned_data'
    # array.

    logging.debug('Errors:')
    if form._errors:
        for k,v in form._errors.items():
            logging.debug('  %s = %s' % (str(k), str(v)))

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

    ctxt = {
        'form' : form,
        'scale_ul' : r0txt,
        'scale_ee' : r1txt,
        'datasrc_url' : ds0,
        'datasrc_file' : ds1,
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

    t = loader.get_template('portal/newjoblong2.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))
