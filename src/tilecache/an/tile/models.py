from django.db import models
import orderby
import logging

class Image(models.Model):
	objects = orderby.OrderByManager()
	
	# The original image filename.
	origfilename = models.CharField(maxlength=1024)
	# The original image file format.
	origformat = models.CharField(maxlength=30)
	# The JPEG base filename.
	filename = models.CharField(maxlength=1024)
	ramin =	 models.DecimalField(decimal_places=7, max_digits=10)
	ramax =	 models.DecimalField(decimal_places=7, max_digits=10)
	decmin = models.DecimalField(decimal_places=7, max_digits=10)
	decmax = models.DecimalField(decimal_places=7, max_digits=10)
	imagew = models.IntegerField()
	imageh = models.IntegerField()

	def __str__(self):
		return self.origfilename

