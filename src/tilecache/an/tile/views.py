from django.http import HttpResponse
from django.template import Context, loader
from an.tile.models import Image
from django.db.models import Q
import re
import os.path
import os
import popen2
# grab a stash of hash
#import hashlib
# old hash
# md5 was causing Apache segfaults for me...
#import md5
import sha
import logging
import commands

import an.gmaps_config as gmaps_config

logfile        = gmaps_config.logfile
tilerender     = gmaps_config.tilerender
cachedir       = gmaps_config.cachedir
rendercachedir = gmaps_config.rendercachedir
tempdir        = gmaps_config.tempdir

logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s %(levelname)s %(message)s',
                    filename=logfile,
					)

def getbb(request):
	try:
		bb = request.GET['bb']
	except (KeyError):
		raise KeyError('No bb')
	bbvals = bb.split(',')
	if (len(bbvals) != 4):
		raise KeyError('Bad bb')
	longmin  = float(bbvals[0])
	latmin   = float(bbvals[1])
	longmax  = float(bbvals[2])
	latmax   = float(bbvals[3])
	# Flip RA!
	(ramin,  ramax ) = (-longmax, -longmin)
	(decmin, decmax) = ( latmin,   latmax )
	# The Google Maps client treats RA as going from -180 to +180; we prefer
	# to think of it as going from 0 to 360.  In the low value is negative,
	# wrap it around...
	if (ramin < 0.0):
		ramin += 360
		ramax += 360
	return (ramin, ramax, decmin, decmax)

def getimage(request):
	logging.debug("getimage() starting")
	try:
		fn = request.GET['filename']
	except KeyError:
		return HttpResponse('No filename specified.')
	#if (fn.find('..')):
	#	return HttpResponse('".." not allowed in filename.')
	q = list(Image.objects.filter(filename=fn))
	if not len(q):
		return HttpResponse('No such file.')
	img = q[0]

	ctmap = { 'jpeg':'image/jpeg' }
	# Yuck!
	suffixmap = { 'jpeg':'jpg' }
	ct = ctmap[img.origformat]
	suff = suffixmap[img.origformat]
	res = HttpResponse()
	res['Content-Type'] = ct

	path = gmaps_config.imgdir + "/" + img.origfilename + "." + suff
	logging.debug("Opening file " + path)
	f = open(path, "rb")
	res.write(f.read())
	f.close()
	return res

def get_overlapping_images(ramin, ramax, decmin, decmax):
	dec_ok = Image.objects.filter(decmin__lte=decmax, decmax__gte=decmin)
	Q_normal = Q(ramin__lte=ramax) & Q(ramax__gte=ramin)
	# In the database, any image that spans the RA=0 line has its bounds
	# bumped up by 360; therefore every "ramin" value is > 0, but some
	# "ramax" values are > 360.
	raminwrap = ramin + 360
	ramaxwrap = ramax + 360
	Q_wrap   = Q(ramin__lte=ramaxwrap) & Q(ramax__gte=raminwrap)
	inbounds = dec_ok.filter(Q_normal | Q_wrap)
	return inbounds

def imagelist(request):
	logging.debug("imagelist() starting")
	try:
		(ramin, ramax, decmin, decmax) = getbb(request)
	except KeyError, x:
		return HttpResponse(x)
	logging.debug("Bounds: RA [%g, %g], Dec [%g, %g]." % (ramin, ramax, decmin, decmax))
	inbounds = get_overlapping_images(ramin, ramax, decmin, decmax)
	# HACK
	dra = ramax - ramin
	sortbysize = inbounds.order_by_expression('abs(%g - abs(ramax - ramin))' % dra)

	top20 = sortbysize[:20]
	query = top20

	res = HttpResponse()
	res['Content-type'] = 'text/xml'
	res.write('<imagelist>\n')
	for img in query:
		res.write('<image name="%s" />\n' % (img.filename))
	res.write('</imagelist>\n')
	logging.debug("Returning %i files." % len(query))
	return res

def query(request):
	logging.debug('query() starting')
	try:
		(ramin, ramax, decmin, decmax) = getbb(request)
	except KeyError, x:
		return HttpResponse(x)
	try:
		imw = int(request.GET['w'])
		imh = int(request.GET['h'])
		layers = request.GET['layers'].split(',')
	except (KeyError):
		return HttpResponse('No w/h/layers')
	if (imw == 0 or imh == 0):
		return HttpResponse('Bad w or h')
	if (len(layers) == 0):
		return HttpResponse('No layers')

	# Build tilerender command-line.
	# RA,Dec range; image size.
	cmdline = tilerender + (" -x %f -X %f -y %f -Y %f" % (ramin, ramax, decmin, decmax))
	cmdline += (" -w %i -h %i" % (imw, imh))
	# cachedir: -D
	cmdline += (" -D " + rendercachedir)
	# layers: -l
	layerexp = re.compile(r'^\w+$')
	#cmdline += (" -l %s" % lay) for lay in layers if layerexp.match(lay)
	for lay in layers:
		if layerexp.match(lay):
			cmdline += (" -l " + lay)

	# valid filenames regexp
	filenameRE = re.compile(r'^[A-Za-z0-9\./_]+$')

	#if ('userimage' in layers) and ('imagefn' in request.GET) and ('wcsfn' in request.GET):
	if ('imagefn' in request.GET) and ('wcsfn' in request.GET):
		img = request.GET['imagefn']
		wcs = request.GET['wcsfn']
		if not (filenameRE.match(img) and filenameRE.match(wcs)):
			logging.debug("Bad image or WCS filename: \"" + img + "\", \"" + wcs + "\".")
			return HttpResponse('bad filename.')
		cmdline += (" -i " + img + " -I " + wcs)

	if ('boundary' in layers) and ('wcsfn' in request.GET):
		wcs = request.GET['wcsfn']
		if not (filenameRE.match(wcs)):
			logging.debug("Bad WCS filename: \"" + wcs + "\".")
			return HttpResponse('bad filename.')
		cmdline += (" -I " + wcs)

	if ('rdlsfn' in request.GET) and ('rdls' in layers):
		rdlsfns = request.GET['rdlsfn'].split(',')
		for rdls in rdlsfns:
			if not filenameRE.match(rdls):
				logging.debug("Bad RDLS filename: \"" + rdls + "\".");
				return HttpResponse('bad filename.')
			cmdline += (' -r ' + rdls)

	if ('images' in layers) or (('boundary' in layers) and not ('wcsfn' in request.GET)):
		# filelist: -S
		# Compute list of files via DB query
		imgs = get_overlapping_images(ramin, ramax, decmin, decmax)
		# Get list of filenames
		filenames = [img.filename for img in imgs]
		files = "\n".join(filenames) + "\n"

		logging.debug("For RA in [%f, %f] and Dec in [%f, %f], found %i files." %
					  (ramin, ramax, decmin, decmax, len(filenames)))

		# Compute filename
		#m = hashlib.md5()
		m = sha.new()
		m.update(files)
		digest = m.hexdigest()
		fn = tempdir + '/' + digest
		# Write to that filename
		try:
			f = open(fn, 'wb')
			f.write(files)
			f.close()
		except (IOError):
			return HttpResponse('Failed to write file list.')
		cmdline += (" -S " + fn)

	# Options with no args:
	optflags = { 'jpeg'   : '-J',
				 'arcsinh': '-s',
				 }
	for opt,arg in optflags.iteritems():
		if (opt in request.GET):
			cmdline += (' ' + arg)

	# Options with numeric args.
	optnum = { 'dashbox' : '-B',
			   'gain'    : '-g',
			   }
	for opt,arg in optnum.iteritems():
		if (opt in request.GET):
			num = float(request.GET[opt])
			cmdline += (" %s %f" % (arg, num))

	# Options with choice args.
	optchoice = { 'colormap' : {'arg':'-C', 'valid':['rb', 'i']},
				  }

	for opt,choice in optchoice.iteritems():
		if (opt in request.GET):
			val = request.GET[opt]
			valid = choice['valid']
			if (val in valid):
				arg = choice['arg']
				cmdline += (' ' + arg + ' ' + val)

	jpeg = ('jpeg' in request.GET)

	res = HttpResponse()
	if jpeg:
		res['Content-Type'] = 'image/jpeg'
	else:
		res['Content-Type'] = 'image/png'
	#logging.debug('command-line is ' + cmdline)

	if ('tag' in request.GET):
		tag = request.GET['tag']
		if not re.match('^\w+$', tag):
			return HttpResponse('Bad tag')
		tilecachedir = cachedir + '/' + tag
		if not os.path.exists(tilecachedir):
			os.mkdir(tilecachedir)

		# Compute filename
		#m = hashlib.md5()
		m = sha.new()
		m.update(cmdline)
		fn = tilecachedir + '/' + 'tile-' + m.hexdigest() + '.'
		if jpeg:
			fn += 'jpg'
		else:
			fn += 'png'

		logging.debug('tilecache file: ' + fn)
		if not os.path.exists(fn):
			# Run it!
			cmd = cmdline + ' > ' + fn + ' 2>> ' + logfile
			logging.debug('running: ' + cmd)
			rtn = os.system(cmd)
			if not(os.WIFEXITED(rtn) and (os.WEXITSTATUS(rtn) == 0)):
				if (os.WIFEXITED(rtn)):
					logging.debug('exited with status %d' % os.WEXITSTATUS(rtn))
				else:
					logging.debug('command did not exit.')
				try:
					os.remove(fn)
				except (os.OSError):
					pass
				return HttpResponse('tilerender command failed.')
		else:
			# Cache hit!
			logging.debug('cache hit!')
			pass

		logging.debug('reading cache file ' + fn)
		f = open(fn, 'rb')
		res.write(f.read())
		f.close()
	else:
		cmd = cmdline + ' 2>> ' + logfile
		logging.debug('no caching: just running command ' + cmd)
		(rtn, out) = commands.getstatusoutput(cmd)
		if not(os.WIFEXITED(rtn) and (os.WEXITSTATUS(rtn) == 0)):
			if (os.WIFEXITED(rtn)):
				logging.debug('exited with status %d' % os.WEXITSTATUS(rtn))
			else:
				logging.debug('command did not exit.')
			return HttpResponse('tilerender command failed.')
		res.write(out)

	logging.debug('finished.')
	return res
