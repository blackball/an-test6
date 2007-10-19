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
		if value.startswith('http://http://') or value.startswith('http://ftp://'):
			value = value[7:]
		return super(ForgivingURLField, self).clean(value)

class SimpleURLForm(forms.Form):
	url = ForgivingURLField(initial='http://',
							widget=forms.TextInput(attrs={'size':'50'}))

class SimpleFileForm(forms.Form):
	file = forms.FileField(widget=forms.FileInput(attrs={'size':'40'}))

#class FullForm(forms.Form):


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
	if (field.verbose_name == 'parity'):
		default.widget = widgets.RadioSelect(choices = field.choices)
	if (field.verbose_name == 'scaleunits' or
		field.verbose_name == 'scaletype' or
		field.verbose_name == 'xysrc'):
		default.widget = widgets.Select(choices = field.choices)
	return default

def newlong(request):
	if not request.user.is_authenticated():
		return HttpResponseRedirect('/login')

	#JobForm = forms.form_for_model(Job)
	newjob = Job()
	JobForm = forms.form_for_instance(newjob, formfield_callback=longform_formfield)

	print 'default parity ', newjob.parity

	if request.POST:
		form = JobForm(request.POST)
	else:
		form = JobForm()

	t = loader.get_template('portal/newjoblong.html')
	c = RequestContext(request, {
		'form' : form,
		})
	return HttpResponse(t.render(c))

def submit(request):
	if not request.user.is_authenticated():
		return HttpResponseRedirect('/login')
	return HttpResponse(request.session['url'] or 'none')
	#return HttpResponse(request.POST['url'])

