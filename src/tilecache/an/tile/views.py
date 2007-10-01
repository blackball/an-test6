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

logfile = "/home/gmaps/test/tilecache-django.log"

logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s %(levelname)s %(message)s',
                    filename=logfile,
					)

def query(request):

	tilerender = "/home/gmaps/test/usnob-map/execs/tilerender"
	cachedir = "/data2/test-tilecache"
	rendercachedir = "/data2/test-rendercache/"
	tempdir = "/tmp"

	logging.debug('starting')

	try:
		bb = request.GET['BBOX']
		imw = int(request.GET['WIDTH'])
		imh = int(request.GET['HEIGHT'])
		layers = request.GET['LAYERS'].split(',')
	except (KeyError):
		return HttpResponse('No BBOX/WIDTH/HEIGHT/LAYERS')
	bbvals = bb.split(',')
	if (len(bbvals) != 4):
		return HttpResponse('Bad BBOX')
	if (imw == 0 or imh == 0):
		return HttpResponse('Bad WIDTH or HEIGHT')
	if (len(layers) == 0):
		return HttpResponse('No LAYERs')
	ramin  = float(bbvals[0])
	decmin = float(bbvals[1])
	ramax  = float(bbvals[2])
	decmax = float(bbvals[3])

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

	#logging.debug('1: cmdline ' + cmdline)

	if ('apod' in layers):
		# filelist: -S
		# Compute list of files via DB query
		dec_ok = Image.objects.filter(decmin__lte=decmax, decmax__gte=decmin)
		Q_normal = Q(ramin__lte=ramax) & Q(ramax__gte=ramin)
		raminwrap = ramin - 360
		ramaxwrap = ramax - 360
		Q_wrap   = Q(ramin__lte=ramaxwrap) & Q(ramax__gte=raminwrap)
		imgs = dec_ok.filter(Q_normal | Q_wrap)
		# Get list of filenames
		filenames = [img.filename for img in imgs]
		files = "\n".join(filenames) + "\n"

		#logging.debug('1.5: files ' + files)
		# Compute filename
		#m = hashlib.md5()
		#logging.debug('1.51: new')
		m = sha.new()
		#logging.debug('1.52: update')
		m.update(files)
		#logging.debug('1.53: dig')
		digest = m.hexdigest()
		#logging.debug('digest: ' + digest)
		#logging.debug('1.54: fn')
		fn = tempdir + '/' + digest
		#logging.debug('1.55: done')
		# Write to that filename
		#logging.debug('1.6: fn ' + fn)
		try:
			#logging.debug('1.61: open')
			f = open(fn, 'wb')
			#logging.debug('1.62: write')
			f.write(files)
			#logging.debug('1.63: close')
			f.close()
			#logging.debug('1.64: done')
		except (IOError):
			return HttpResponse('Failed to write file list.')
		cmdline += (" -S " + fn)

	#logging.debug('2: cmdline ' + cmdline)

	optflags = { 'outline': '-O',
				 'jpeg'   : '-J',
				 'arcsinh': '-s',
				 }
	for opt,arg in optflags.iteritems():
		if (opt in request.GET):
			cmdline += (' ' + arg)

	#logging.debug('2.5: cmdline ' + cmdline)

	optnum = { 'dashbox' : '-B',
			   'gain'    : '-g',
			   }
	for opt,arg in optnum.iteritems():
		if (opt in request.GET):
			num = float(request.GET[opt])
			cmdline += (" %s %f" % arg, num)

	#logging.debug('3: cmdline ' + cmdline)

	optchoice = { 'colormap' : {'arg':'-C', 'valid':['rb', 'i']},
				  }

	for opt,choice in optchoice.iteritems():
		if (opt in request.GET):
			val = request.GET[opt]
			valid = choice['valid']
			if (val in valid):
				arg = choice['arg']
				cmdline += (' ' + arg + ' ' + val)

	#logging.debug('4: cmdline ' + cmdline)

	jpeg = ('jpeg' in request.GET)

	res = HttpResponse()
	if jpeg:
		res['Content-Type'] = 'image/jpeg'
	else:
		res['Content-Type'] = 'image/png'
	#res['Content-Type'] = (jpeg ? 'image/jpeg' : 'image/png')

	#logging.debug('command-line is ' + cmdline)

	if ('tag' in request.GET):
		tag = request.GET['tag']
		if not re.match('^\w+$'):
			return HttpResponse('Bad tag')
		tilecachedir = cachedir + '/' + tag
		if not os.path.exists(tilecachedir):
			os.mkdir(tilecachedir)

		# Compute filename
		#m = hashlib.md5()
		m = sha.new()
		m.update(cmdline)
		fn = tilecachedir + '/' + 'tile-' + m.hexdigest() + '.'
		# + (jpeg ? 'jpg' : 'png')
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
		child = popen2.Popen3(cmd, False)
		child.tochild.close()
		rtn = child.wait()
		if not(os.WIFEXITED(rtn) and (os.WEXITSTATUS(rtn) == 0)):
			if (os.WIFEXITED(rtn)):
				logging.debug('exited with status %d' % os.WEXITSTATUS(rtn))
			else:
				logging.debug('command did not exit.')
			return HttpResponse('tilerender command failed.')
		res.write(child.fromchild.read())
		child.fromchild.close()

	logging.debug('finished.')
	return res
