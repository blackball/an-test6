from django.conf.urls.defaults import *
from an import settings

urlpatterns = patterns('',
					   (r'^tile/', include('an.tile.urls')),
					   (r'^upload/', include('an.upload.urls')),
					   (r'^login/', 'an.portal.views.login'),
					   (r'^logout/', 'an.portal.views.logout'),
					   (r'^job/', include('an.portal.urls')),
					   (r'^userprefs/', 'an.portal.views.userprefs'),
					   (r'^vo/', include('an.vo.urls')),
                       (r'^testbed/', include('an.testbed.urls')),
                       # This is a fake placeholder to allow {% url %} and reverse() to resolve an.media to /anmedia.
                       # (see also media() in an/__init__.py)
                       (r'^anmedia/', 'an.media'),
					   )


