from django.conf.urls.defaults import *

urlpatterns = patterns('an.portal.views',
					   (r'^newurl/$', 'newurl'),
					   (r'^newfile/$', 'newfile'),
					   (r'^newlong/$', 'newlong'),
					   #(r'^newbare/$', 'newbare'),
					   (r'^status/$', 'jobstatus'),
					   (r'^getfile/$', 'getfile'),
					   (r'^summary/$', 'summary'),
					   )
