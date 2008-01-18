from django.conf.urls.defaults import *
from an import settings

urlpatterns = patterns('',
					   (r'^tile/', include('an.tile.urls')),
					   (r'^upload/', include('an.upload.urls')),
					   (r'^login/', 'django.contrib.auth.views.login',
                        {'template_name': 'portal/login.html'}),
                       (r'^logout/', 'django.contrib.auth.views.logout_then_login'),
					   (r'^userprefs/', 'an.portal.views.userprefs'),
                       (r'^changepassword/',  'django.contrib.auth.views.password_change',
                        {'template_name': 'portal/changepassword.html'}),
                       (r'^changedpassword/', 'django.contrib.auth.views.password_change_done'),
                       (r'^resetpassword/',   'django.contrib.auth.views.password_reset'),
                       (r'^job/', include('an.portal.urls')),
					   (r'^vo/', include('an.vo.urls')),
                       (r'^testbed/', include('an.testbed.urls')),
                       # This is a fake placeholder to allow {% url %} and reverse() to resolve an.media to /anmedia.
                       # (see also media() in an/__init__.py)
                       (r'^anmedia/', 'an.media'),
                       (r'^logout/', 'an.logout'),
                       (r'^login/', 'an.login'),
                       (r'^changepassword/',  'an.changepassword'),
                       (r'^changedpassword/', 'an.changedpassword'),
                       (r'^resetpassword/',   'an.resetpassword'),
					   )


