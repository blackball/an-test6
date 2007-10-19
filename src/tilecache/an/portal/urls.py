from django.conf.urls.defaults import *

urlpatterns = patterns('an.portal.views',
					   (r'^new/$', 'new'),
					   (r'^submit/$', 'submit'),
					   )
