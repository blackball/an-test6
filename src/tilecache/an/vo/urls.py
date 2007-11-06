from django.conf.urls.defaults import *

urlpatterns = patterns('an.vo.views',
					   #(r'^siap(-pointed)?/$', 'siap'),
					   (r'^siap-pointed/$', 'siap_pointed'),
					   (r'^getimage/$', 'getimage'),
                       )
