from an.portal.models import Job
from django import newforms as forms
from django.http import HttpResponse
from django.http import HttpResponseRedirect
from django.template import Context, RequestContext, loader
from django.contrib.auth import authenticate
from django.contrib.auth import login as authlogin
from django.contrib.auth import logout as authlogout

# Adding a user:
# > python manage.py shell
# >>> from django.contrib.auth.models import User
# >>> user = User.objects.create_user('john', 'lennon@thebeatles.com', 'johnpassword')
# >>> user.save()

class LoginForm(forms.Form):
    username = forms.EmailField()
    password = forms.CharField(max_length=100, widget=forms.PasswordInput)

def login(request, redirect_to=None):
	form = LoginForm(request.POST)
	authfailed = False
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
				# Failure
				authfailed = True

	t = loader.get_template('portal/login.html')
	c = Context({
		'form' : form,
		})
	return HttpResponse(t.render(c))

def logout(request):
    if not request.user.is_authenticated():
        return HttpResponse("piss off.")
    authlogout(request)
    return HttpResponseRedirect('/login')

def new(request):
	form = LoginForm(request.POST)
	t = loader.get_template('portal/newjob.html')
	c = RequestContext(request, {
		'form' : form,
		})
	return HttpResponse(t.render(c))

