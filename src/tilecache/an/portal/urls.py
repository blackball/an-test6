from django.conf.urls.defaults import *

urlpatterns = patterns('an.portal.views',
					   (r'^newurl/$', 'newurl'),
					   (r'^newfile/$', 'newfile'),
					   (r'^newlong/$', 'newlong'),
					   (r'^newlong2/$', 'newlong2'),
					   (r'^submit/$', 'submit'),
					   )
