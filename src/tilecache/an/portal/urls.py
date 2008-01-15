from django.conf.urls.defaults import *

#from an.portal import newjob
import an.portal.newjob
import an.portal.views


urlpatterns = patterns('',
					   (r'^newurl/$',      an.portal.newjob.newurl),
					   (r'^newfile/$',     an.portal.newjob.newfile),
					   (r'^newlong/$',     an.portal.newjob.newlong),
					   (r'^status/$',      an.portal.views.jobstatus),
					   (r'^getfile/$',     an.portal.views.getfile),
					   (r'^summary/$',     an.portal.views.summary),
					   (r'^changeperms/$', an.portal.views.changeperms),
					   (r'^publishtovo/$', an.portal.views.publishtovo),
					   )
