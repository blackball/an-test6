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
					   (r'^set_description/$', an.portal.views.job_set_description),
					   (r'^add_tag/$', an.portal.views.job_add_tag),
					   (r'^remove_tag/$', an.portal.views.job_remove_tag),
					   (r'^tag_summary/$',     an.portal.views.tag_summary),
					   (r'^user_summary/$',     an.portal.views.user_summary),
                       (r'^run-variant/$', an.portal.views.run_variant),
					   (r'^changeperms/$', an.portal.views.changeperms),
					   (r'^publishtovo/$', an.portal.views.publishtovo),
					   )
