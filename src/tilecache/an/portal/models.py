from django.db import models
from django.contrib.auth.models import User

# To install a new database table:
# > python manage.py sql portal
# > sqlite3 /data2/test-django/tile.db
# sqlite> drop table portal_job;
# sqlite>   (paste CREATE TABLE statement)

class Job(models.Model):
	xysrc_CHOICES = (
		('url', 'URL'),
		('file', 'File'),
		('fitsfile', 'FITS file'),
		)

	scaleunit_CHOICES = (
		('arcsecperpix', 'arcseconds per pixel'),
		('arcminwidth' , 'width of the field (in arcminutes)'), 
		('degreewidth' , 'width of the field (in degrees)'),
		('focalmm'     , 'focal length of the lens (for 35mm film equivalent sensor)'),
		)

	scaletype_CHOICES = (
		('ul', 'lower and upper bounds'),
		('ev', 'estimate and error bound'),
		)

	parity_CHOICES = (
		('pos', 'Positive'),
		('neg', 'Negative'),
		('both', 'Try both'),
		)

	status_CHOICES = (
		('not-submitted', 'Submission failed'),
		('queued', 'Queued'),
		('started', 'Started'),
		('solved', 'Solved'),
		('failed', 'Failed')
		)

	user = models.ForeignKey(User)
	xysrc = models.CharField(max_length=16, choices=xysrc_CHOICES)
	url = models.URLField()

	jobid = models.CharField(max_length=32)

	# filename of the uploaded file on the user's machine.
	userfilename = models.CharField(max_length=256)
	# content-type of the uploaded file, if it was compressed
	compressedtype = models.CharField(max_length=64)
	# content-type of the uploaded file, after compression
	filetype = models.CharField(max_length=64)

	# sha-1 hash of the uncompressed file.
	filehash = models.CharField(max_length=40)

	# for FITS tables, the names of the X and Y columns.
	xcol = models.CharField(max_length=16)
	ycol = models.CharField(max_length=16)

	# size of the field
	imagew = models.PositiveIntegerField()
	imageh = models.PositiveIntegerField()

	# scale factor for rendering images; <=1.
	displayscale = models.DecimalField(max_digits=10, decimal_places=10)
	# size to render images.
	displayw = models.PositiveIntegerField()
	displayh = models.PositiveIntegerField()

	parity = models.CharField(max_length=5, choices=parity_CHOICES)

	# image scale.
	scaleunits = models.CharField(max_length=5, choices=scaleunit_CHOICES)
	scaletype  = models.CharField(max_length=3, choices=scaletype_CHOICES)
	scalelower = models.DecimalField(max_digits=20, decimal_places=10)
	scaleupper = models.DecimalField(max_digits=20, decimal_places=10)
	scaleest   = models.DecimalField(max_digits=20, decimal_places=10)
	scaleerr   = models.DecimalField(max_digits=20, decimal_places=10)

	# tweak.
	tweak = models.BooleanField()
	tweakorder = models.PositiveSmallIntegerField()

	#
	status = models.CharField(max_length=16, choices=status_CHOICES)
	failurereason = models.CharField(max_length=256)

	# times
	submittime = models.DateTimeField()
	starttime  = models.DateTimeField()
	finishtime = models.DateTimeField()

	## These fields don't go in the database.

	# filename of the uploaded file on the local machine.
	localfilename = None

