from django.conf.urls.defaults import *

urlpatterns = patterns('an.portal.views',
					   (r'^newurl/$', 'newurl'),
					   (r'^newfile/$', 'newfile'),
					   (r'^newlong/$', 'newlong'),
                       # TEMP
					   (r'^newlong2/$', 'newlong'),
					   #(r'^upload/$', 'upload'),
					   (r'^uploadform/$', 'uploadform'),
					   (r'^uploadprogress/$', 'uploadprogress'),
					   (r'^submit/$', 'submit'),
					   )
