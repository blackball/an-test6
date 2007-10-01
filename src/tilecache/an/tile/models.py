from django.db import models

class Image(models.Model):
	# The original image filename.
	origfilename = models.CharField(maxlength=1024)
	# The original image file format.
	origformat = models.CharField(maxlength=30)
	# The JPEG base filename.
	filename = models.CharField(maxlength=1024)
	ramin =  models.FloatField(decimal_places=7, max_digits=10)
	ramax =  models.FloatField(decimal_places=7, max_digits=10)
	decmin = models.FloatField(decimal_places=7, max_digits=10)
	decmax = models.FloatField(decimal_places=7, max_digits=10)
	imagew = models.IntegerField()
	imageh = models.IntegerField()

	def __str__(self):
		return self.basefilename

	# Include it in the Django admin page?
	#class Admin:
	#	pass

