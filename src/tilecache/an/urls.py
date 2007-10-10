from django.conf.urls.defaults import *

urlpatterns = patterns('an.tile.views',
					   (r'^tile/get/$', 'query'),
					   (r'^tile/list/$', 'imagelist'),
)
