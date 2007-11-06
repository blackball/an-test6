from django.conf.urls.defaults import *

urlpatterns = patterns('an.portal',
					   (r'^newurl/$', 'newjob.newurl'),
					   (r'^newfile/$', 'newjob.newfile'),
					   (r'^newlong/$', 'newjob.newlong'),
					   #(r'^newbare/$', 'newbare'),
					   (r'^status/$', 'views.jobstatus'),
					   (r'^getfile/$', 'views.getfile'),
					   (r'^summary/$', 'views.summary'),
					   (r'^changeperms/$', 'views.changeperms'),
					   )
