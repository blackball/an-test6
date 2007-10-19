from django.conf.urls.defaults import *
import settings

urlpatterns = patterns('',
					   (r'^tile/', include('an.tile.urls')),
					   (r'^login/', 'an.portal.views.login'),
					   (r'^logout/', 'an.portal.views.logout'),
					   (r'^job/', include('an.portal.urls')),
					   (r'^admin/', include('django.contrib.admin.urls')),
					   (r'^anmedia/(?P<path>.*)$', 'django.views.static.serve', {'document_root' : settings.MEDIA_ROOT}),
					   )


