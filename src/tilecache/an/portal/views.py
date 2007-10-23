from an.portal.models import Job
from django import newforms as forms
from django.http import HttpResponse
from django.http import HttpResponseRedirect
from django.template import Context, RequestContext, loader
from django.contrib.auth import authenticate
from django.contrib.auth import login as authlogin
from django.contrib.auth import logout as authlogout
from django.newforms import widgets
import settings

# Adding a user:
# > python manage.py shell
# >>> from django.contrib.auth.models import User
# >>> user = User.objects.create_user('john', 'lennon@thebeatles.com', 'johnpassword')
# >>> user.save()

class LoginForm(forms.Form):
	username = forms.EmailField()
	password = forms.CharField(max_length=100, widget=forms.PasswordInput)

class ForgivingURLField(forms.URLField):
	def clean(self, value):
		print 'cleaning value', value
		if value is not None and \
			   (value.startswith('http://http://') or value.startswith('http://ftp://')):
			value = value[7:]
		return super(ForgivingURLField, self).clean(value)

class SimpleURLForm(forms.Form):
	url = ForgivingURLField(initial='http://',
							widget=forms.TextInput(attrs={'size':'50'}))

class SimpleFileForm(forms.Form):
	file = forms.FileField(widget=forms.FileInput(attrs={'size':'40'}))

class FullForm(forms.Form):
	xysrc = forms.ChoiceField(choices=Job.xysrc_CHOICES,
							  initial='url',
							  widget=forms.Select(
		attrs={'onchange':'sourceChanged()',
			   'onkeyup':'sourceChanged()'}))
	scaleunits = forms.ChoiceField(choices=Job.scaleunit_CHOICES,
								   widget=forms.Select(
		attrs={'onchange':'unitsChanged()',
			   'onkeyup':'unitsChanged()'}))
	scaletype = forms.ChoiceField(choices=Job.scaletype_CHOICES,
								  widget=forms.RadioSelect(),
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
			   }), required=False)
	scaleerr = forms.DecimalField(widget=forms.TextInput(
		attrs={'onfocus':'setFsEv()',
			   'onkeyup':'scalechanged()',
			   'size':'5',
			   }), required=False, min_value=0, max_value=100)
	file = forms.FileField(widget=forms.FileInput(attrs={'size':'40'}),
						   required=False)
	url = ForgivingURLField(initial='http://',
							widget=forms.TextInput(attrs={'size':'50'}),
							required=False)
	xcol = forms.CharField(initial='X',
						   required=False,
						   widget=forms.TextInput(attrs={'size':'10'}))
	ycol = forms.CharField(initial='Y',
						   required=False,
						   widget=forms.TextInput(attrs={'size':'10'}))

	tweak = forms.BooleanField(initial=True)

	tweakorder = forms.IntegerField(initial=2, min_value=2,
                                    max_value=10,
                                    widget=forms.TextInput(attrs={'size':'5'}))

	def getclean(self, name):
		if name in self.cleaned_data:
			return self.cleaned_data[name]
		return None
	def geterror(self, name):
		if name in self._errors:
			return self._errors[name]
		return None

	def clean(self):
		"""Take a shower"""
		xysrc = self.getclean('xysrc')
		if (xysrc == 'url') or (xysrc == 'fitsurl'):
			urlerr = self.geterror('url')
			urlval = self.getclean('url')
			print 'url error:', urlerr
			print 'url value:', urlval
			if urlerr is None:
				if urlval is None or (len(urlval) == 0):
					self._errors['url'] = ['URL is required']
		elif (xysrc == 'file') or (xysrc == 'fitsfile'):
			if (not 'file' in self._errors):
				file = self.getclean('file')
				print 'file is',repr(file)
				print 'file in self.files is',repr('file' in self.files and self.files['file'] or None)
				if not file:
					self._errors['file'] = ['You must upload a file']
		return self.cleaned_data
		



def login(request, redirect_to=None):
	form = LoginForm(request.POST)
	authfailed = False
	usererr = None
	passerr = None
	if form.is_valid():
		username = form.cleaned_data['username']
		password = form.cleaned_data['password']

		user = authenticate(username=username, password=password)
		if user is not None:
			if user.is_active:
				authlogin(request, user)
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
	authlogout(request)
	return HttpResponseRedirect('/login')

def newurl(request):
	if not request.user.is_authenticated():
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
		form = FullForm(request.POST, request.FILES)
	else:
		form = FullForm()

	# Ugh, special rendering for radio checkboxes....
	scaletype = form['scaletype'].field
	attrs = scaletype.widget.attrs
	render = form['scaletype'].field.widget.get_renderer('scaletype',
														 scaletype.initial,
														 attrs)
	r0 = render[0]
	r0.attrs['id'] = 'scaletype'
	r0.attrs['onclick'] = 'scalechanged()'
	r1 = render[1]
	r1.attrs['id'] = 'scaletype'
	r1.attrs['onclick'] = 'scalechanged()'
	r0txt = r0.tag()
	r1txt = r1.tag()

	ctxt = {
		'form' : form,
		'scale_ul' : r0txt,
		'scale_ee' : r1txt,
		}
	errfields = [ 'scalelower', 'scaleupper', 'scaleest', 'scaleerr' ]
	for f in errfields:
		ctxt[f + '_err'] = len(form[f].errors) and form[f].errors[0] or None

	if request.POST:
		form.is_valid()

		if request.FILES:
			f1 = request.FILES['file']
			print 'file is', repr(f1)
		else:
			print 'no file uploaded'

		urlerr	= len(form['url'].errors)  and form['url'].errors [0] or None
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
	return HttpResponse(request.session['url'] or 'none')
	#return HttpResponse(request.POST['url'])

