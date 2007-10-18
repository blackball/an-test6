from django.conf.urls.defaults import *

urlpatterns = patterns('an.tile.views',
					   (r'^get/$', 'query'),
					   (r'^list/$', 'imagelist'),
					   (r'^image/$', 'getimage'),
					   )
