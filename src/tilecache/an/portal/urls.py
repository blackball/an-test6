from django.conf.urls.defaults import *

urlpatterns = patterns('an.portal.views',
					   (r'^newurl/$', 'newurl'),
					   (r'^newfile/$', 'newfile'),
					   (r'^newlong/$', 'newlong'),
                       # TEMP
					   (r'^newlong2/$', 'newlong'),
					   (r'^submit/$', 'submit'),
					   )
