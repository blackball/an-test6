from an.portal.models import Job
from django import newforms as forms
from django.http import HttpResponse
from django.http import HttpResponseRedirect
from django.template import Context, RequestContext, loader
from django.contrib.auth import authenticate
from django.contrib.auth import login as authlogin
from django.contrib.auth import logout as authlogout
from django.newforms import widgets
import re
import settings

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

	tweak = forms.BooleanField(initial=True, required=False)

	tweakorder = forms.IntegerField(initial=2, min_value=2,
									max_value=10,
									widget=forms.TextInput(attrs={'size':'5'}))

	# How to clean an individual field:
	#def clean_scaletype(self):
	#	val = self.getclean('scaletype')
	#	return val

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
		#print 'request.POST is', request.POST
		print 'POST values:'
		for k,v in request.POST.items():
			print '		 ', k, '=', v
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

	print 'Errors:'
	if form._errors:
		for k,v in form._errors.items():
			print '  ',k,'=',v

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

	txt = "<pre>Job values:\n"
	for k,v in request.session['jobvals'].items():
		txt += '  ' + str(k) + ' = ' + str(v) + '\n'
	txt += "</pre>"
	return HttpResponse(txt)

