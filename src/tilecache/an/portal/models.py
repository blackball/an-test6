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
		('fitsfile', 'FITS table file'),
		('fitsurl', 'FITS table URL'),
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

	#parity_CHOICES = (
	#	('pos', 'Positive'),
	#	('neg', 'Negative'),
	#	('both', 'Try both'),
	#	)

	parity_CHOICES = (
		(0, 'Positive'),
		(1, 'Negative'),
		(2, 'Try both simultaneously'),
		)

	status_CHOICES = (
		('not-submitted', 'Submission failed'),
		('queued', 'Queued'),
		('started', 'Started'),
		('solved', 'Solved'),
		('failed', 'Failed')
		)

	jobid = models.CharField(max_length=32, unique=True, editable=False,
							 primary_key=True)

	user = models.ForeignKey(User, editable=False)
	xysrc = models.CharField(max_length=16, choices=xysrc_CHOICES)
	url = models.URLField(blank=True)

	# filename of the uploaded file on the user's machine.
	userfilename = models.CharField(max_length=256, editable=False, blank=True)
	# content-type of the uploaded file, if it was compressed
	compressedtype = models.CharField(max_length=64, editable=False, blank=True)
	# content-type of the uploaded file, after compression
	filetype = models.CharField(max_length=64, editable=False)

	# sha-1 hash of the uncompressed file.
	filehash = models.CharField(max_length=40, editable=False)

	# for FITS tables, the names of the X and Y columns.
	xcol = models.CharField(max_length=16, blank=True)
	ycol = models.CharField(max_length=16, blank=True)

	# size of the field
	imagew = models.PositiveIntegerField(editable=False)
	imageh = models.PositiveIntegerField(editable=False)

	# scale factor for rendering images; <=1.
	displayscale = models.DecimalField(max_digits=10, decimal_places=10, editable=False)
	# size to render images.
	displayw = models.PositiveIntegerField(editable=False)
	displayh = models.PositiveIntegerField(editable=False)

	#parity = models.CharField(max_length=5, choices=parity_CHOICES,
	#						  default='both')
	parity = models.PositiveSmallIntegerField(choices=parity_CHOICES,
											  default=2, radio_admin=True,
											  core=True)

	# image scale.
	scaleunits = models.CharField(max_length=16, choices=scaleunit_CHOICES,
								  default='degreewidth')
	scaletype  = models.CharField(max_length=3, choices=scaletype_CHOICES,
								  default='ul')
	scalelower = models.DecimalField(max_digits=20, decimal_places=10,
									 default=0.1, blank=True)
	scaleupper = models.DecimalField(max_digits=20, decimal_places=10,
									 default=180, blank=True)
	scaleest   = models.DecimalField(max_digits=20, decimal_places=10, blank=True)
	scaleerr   = models.DecimalField(max_digits=20, decimal_places=10, blank=True)

	# tweak.
	tweak = models.BooleanField(default=True)
	tweakorder = models.PositiveSmallIntegerField(default=2)

	#
	status = models.CharField(max_length=16, choices=status_CHOICES, editable=False)
	failurereason = models.CharField(max_length=256, editable=False)

	# times
	submittime = models.DateTimeField(editable=False)
	starttime  = models.DateTimeField(editable=False)
	finishtime = models.DateTimeField(editable=False)

	## These fields don't go in the database.

	# filename of the uploaded file on the local machine.
	localfilename = None

