from an.portal.models import Job
from django import newforms as forms
from django.http import HttpResponse
from django.http import HttpResponseRedirect
from django.template import Context, RequestContext, loader
from django.contrib.auth import authenticate
from django.contrib.auth import login as authlogin
from django.contrib.auth import logout as authlogout
import settings

# Adding a user:
# > python manage.py shell
# >>> from django.contrib.auth.models import User
# >>> user = User.objects.create_user('john', 'lennon@thebeatles.com', 'johnpassword')
# >>> user.save()

class LoginForm(forms.Form):
	username = forms.EmailField()
	password = forms.CharField(max_length=100, widget=forms.PasswordInput)

class SimpleURLForm(forms.Form):
	url = forms.URLField(initial='http://',
						 widget=forms.TextInput(attrs={'size':'50'}))
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
				return HttpResponseRedirect('/job/new')
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

def new(request):
	if not request.user.is_authenticated():
		return HttpResponseRedirect('/login')
	urlerr = None
	if len(request.POST):
		form = SimpleURLForm(request.POST)
		if form.is_valid():
			url = form.cleaned_data['url']
			return HttpResponseRedirect('/job/submit')
		else:
			urlerr = form['url'].errors[0]
	else:
		form = SimpleURLForm()
		
	t = loader.get_template('portal/newjob.html')
	c = RequestContext(request, {
		'form' : form,
		'urlerr' : urlerr,
		})
	return HttpResponse(t.render(c))

