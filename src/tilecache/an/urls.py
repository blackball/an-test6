from django.conf.urls.defaults import *
from an import settings

urlpatterns = patterns('',
					   (r'^tile/', include('an.tile.urls')),
					   (r'^upload/', include('an.upload.urls')),
					   (r'^login/', 'an.portal.views.login'),
					   (r'^logout/', 'an.portal.views.logout'),
					   (r'^job/', include('an.portal.urls')),
                       # DEBUG
					   (r'^upload-demo/', 'an.uploaddemo.view'),
#					   (r'^admin/', include('django.contrib.admin.urls')),
					   (r'^anmedia/(?P<path>.*)$', 'django.views.static.serve', {'document_root' : settings.MEDIA_ROOT}),
					   )


