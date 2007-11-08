from django.conf.urls.defaults import *

#urlpatterns = patterns('an.portal',
#					   (r'^newurl/$', 'newjob.newurl'),
#					   (r'^newfile/$', 'newjob.newfile'),
#					   (r'^newlong/$', 'newjob.newlong'),
#					   (r'^status/$', 'views.jobstatus'),
#					   (r'^getfile/$', 'views.getfile'),
#					   (r'^summary/$', 'views.summary'),
#					   (r'^changeperms/$', 'views.changeperms'),
#					   )

urlpatterns = patterns('',
					   (r'^newurl/$', 'an.portal.newjob.newurl'),
					   (r'^newfile/$', 'an.portal.newjob.newfile'),
					   (r'^newlong/$', 'an.portal.newjob.newlong'),
					   (r'^status/$', 'an.portal.views.jobstatus'),
					   (r'^getfile/$', 'an.portal.views.getfile'),
					   (r'^summary/$', 'an.portal.views.summary'),
					   (r'^changeperms/$', 'an.portal.views.changeperms'),
					   (r'^publishtovo/$', 'an.portal.views.publishtovo'),
					   )
