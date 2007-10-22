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
							   widget=forms.RadioSelect())
	scalelower = forms.DecimalField(widget=forms.TextInput(
		attrs={'onfocus':'setFsUl()',
			   'onkeyup':'scalechanged()',
			   'size':'5',
			   }), initial=0.1, required=False)
	scaleupper = forms.DecimalField(widget=forms.TextInput(
		attrs={'onfocus':'setFsUl()',
			   'onkeyup':'scalechanged()',
			   'size':'5',
			   }), initial=180, required=False)
	scaleest = forms.DecimalField(widget=forms.TextInput(
		attrs={'onfocus':'setFsEv()',
			   'onkeyup':'scalechanged()',
			   'size':'5',
			   }), required=False)
	scaleerr = forms.DecimalField(widget=forms.TextInput(
		attrs={'onfocus':'setFsEv()',
			   'onkeyup':'scalechanged()',
			   'size':'5',
			   }), required=False)
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

def longform_formfield(field, **kwargs):
	print field.verbose_name
	print 'field is: ', field
	default = field.formfield(**kwargs)
	print 'form  is: ', default
	print 'core? ', field.core
	print
	if (field.verbose_name == 'parity' or
		field.verbose_name == 'scaletype'):
		default.widget = widgets.RadioSelect(choices = field.choices)
	if (field.verbose_name == 'scaleunits' or
		#field.verbose_name == 'scaletype' or
		field.verbose_name == 'xysrc'):
		default.widget = widgets.Select(choices = field.choices)
	return default

def newlong(request):
	if not request.user.is_authenticated():
		return HttpResponseRedirect('/login')

	#JobForm = forms.form_for_model(Job)
	#newjob = Job()
	#JobForm = forms.form_for_instance(newjob, formfield_callback=longform_formfield)
	#if request.POST:
	#	form = JobForm(request.POST)
	#else:
	#	form = JobForm()

	if request.POST:
		form = FullForm(request.POST)
	else:
		form = FullForm()

	#print form.scaletype_0
	#print form
	#print form['scaletype']
	#print form['scaletype'].field
	#print form['scaletype'].field.widget
	scaletype = form['scaletype'].field
	#print
	attrs = scaletype.widget.attrs
	#attrs['id'] = 'scaletype'
	render = form['scaletype'].field.widget.get_renderer('scaletype',
														 scaletype.initial,
														 attrs)
	r0 = render[0]
	r0.attrs['id'] = 'scaletype'
	r0.attrs['onclick'] = 'scalechanged()'
	r1 = render[1]
	r1.attrs['id'] = 'scaletype'
	r1.attrs['onclick'] = 'scalechanged()'
	#print 'choice value:', r1.choice_value
	#print 'value:', r1.value
	#r0txt = str(r0)
	#r1txt = str(r1)
	r0txt = r0.tag()
	r1txt = r1.tag()
	#print str(type(r0))
	#print r0txt
	#print r1txt

	ctxt = {
		'form' : form,
		'scale_ul' : r0txt,
		'scale_ee' : r1txt,
		}
	errfields = [ 'scalelower', 'scaleupper', 'scaleest', 'scaleerr' ]
	for f in errfields:
		ctxt[f + '_err'] = len(form[f].errors) and form[f].errors[0] or None

	urlerr  = len(form['url'].errors)  and form['url'].errors [0] or None
	fileerr = len(form['file'].errors) and form['file'].errors[0] or None
	val = form['xysrc'].field.clean(request.POST['xysrc'])
	#val = form.cleaned_data['xysrc']
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

