from plotstuff_c import *

# Could consider using swig's "addmethods" mechanism to create this "class" rep.

class Plotstuff(object):
	def __init__(self):
		p = plotstuff_new()
		#print 'plotstuff.__init__, pargs=', p
		self.pargs = p
		#self.pargs = plotstuff_new()

	def __del__(self):
		#print 'plotstuff.__del__, pargs=', self.pargs
		plotstuff_free(self.pargs)

	def __getattr__(self, name):
		if name == 'xy':
			return plot_xy_get(self.pargs)
		elif name == 'index':
			return plot_index_get(self.pargs)
		elif name == 'radec':
			return plot_radec_get(self.pargs)
		elif name == 'match':
			return plot_match_get(self.pargs)
		elif name == 'image':
			return plot_image_get(self.pargs)
		elif name == 'outline':
			return plot_outline_get(self.pargs)
		elif name == 'grid':
			return plot_grid_get(self.pargs)
		elif name in ['ann', 'annotations']:
			return plot_annotations_get(self.pargs)
		return self.pargs.__getattr__(name)

	def __setattr__(self, name, val):
		if name == 'pargs':
			#print 'plotstuff.py: setting pargs to', val
			self.__dict__[name] = val
		elif name == 'size':
			#print 'plotstuff.py: setting plot size of', self.pargs, 'to %i,%i' % (val[0], val[1])
			plotstuff_set_size(self.pargs, val[0], val[1])
		elif name == 'color':
			self.set_color(val)
		elif name == 'alpha':
			self.set_alpha(val)
		elif name == 'marker' and type(val) is str:
			plotstuff_set_marker(self.pargs, val)
		elif name == 'wcs_file':
			plotstuff_set_wcs_file(self.pargs, val, 0)
		#elif name == 'operator':
		#	print 'val:', val
		#	self.pargs.op = val
		else:
			self.pargs.__setattr__(name, val)

	def get_image_as_numpy(self):
		return self.pargs.get_image_as_numpy()

	def apply_settings(self):
		plotstuff_builtin_apply(self.pargs.cairo, self.pargs)

	def plot(self, layer):
		plotstuff_plot_layer(self.pargs, layer)

	def scale_wcs(self, scale):
		plotstuff_scale_wcs(self.pargs, scale)

	def rotate_wcs(self, angle):
		plotstuff_rotate_wcs(self.pargs, angle)

	def set_wcs_box(self, ra, dec, width):
		plotstuff_set_wcs_box(self.pargs, ra, dec, width)

	def set_color(self, color):
		x = plotstuff_set_color(self.pargs, color)

	def set_alpha(self, a):
		x = plotstuff_set_alpha(self.pargs, a)

	def plot_grid(self, rastep, decstep, ralabelstep=None, declabelstep=None):
		grid = plot_grid_get(self.pargs)
		grid.rastep = rastep
		grid.decstep = decstep
		if ralabelstep is None:
			ralabelstep = 0
		if declabelstep is None:
			declabelstep = 0
		grid.ralabelstep = ralabelstep
		grid.declabelstep = declabelstep
		self.plot('grid')
		
	def write(self, filename):
		self.outfn = filename
		plotstuff_output(self.pargs)
