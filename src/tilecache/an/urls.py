from django.conf.urls.defaults import *

urlpatterns = patterns('',
					   # Example:
					   # (r'^an/', include('an.foo.urls')),
					   (r'^query/$', 'an.tile.views.query'),
					   # Uncomment this for admin:
					   #     (r'^admin/', include('django.contrib.admin.urls')),
)
