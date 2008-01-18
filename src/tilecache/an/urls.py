from django.conf.urls.defaults import *
from an import settings

urlpatterns = patterns('',
					   (r'^tile/', include('an.tile.urls')),
					   (r'^upload/', include('an.upload.urls')),
					   (r'^login/', 'django.contrib.auth.views.login',
                        {'template_name': 'portal/login.html'}),
                       (r'^logout/', 'django.contrib.auth.views.logout_then_login'),
					   (r'^job/', include('an.portal.urls')),
					   (r'^userprefs/', 'an.portal.views.userprefs'),
					   (r'^vo/', include('an.vo.urls')),
                       (r'^testbed/', include('an.testbed.urls')),
                       # This is a fake placeholder to allow {% url %} and reverse() to resolve an.media to /anmedia.
                       # (see also media() in an/__init__.py)
                       (r'^anmedia/', 'an.media'),
                       (r'^logout/', 'an.logout'),
                       (r'^login/', 'an.login'),
					   )


