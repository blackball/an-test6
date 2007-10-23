from django.conf.urls.defaults import *

#urlpatterns = patterns('an.portal.views2',
#					   (r'^newurl/$', 'newurl'),
#					   (r'^newfile/$', 'newfile'),
#					   (r'^newlong/$', 'newlong'),
#					   (r'^submit/$', 'submit'),
#					   )

urlpatterns = patterns('an.portal.views',
					   (r'^newurl/$', 'newurl'),
					   (r'^newfile/$', 'newfile'),
					   (r'^newlong/$', 'newlong'),
					   (r'^submit/$', 'submit'),
					   )
