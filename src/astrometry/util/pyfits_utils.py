import pyfits
from numpy import array

class tabledata(object):
	def __init__(self):
		self._length = 0
	def __setattr__(self, name, val):
		object.__setattr__(self, name, val)
	def set(self, name,val):
		self.__setattr__(name, val)
	def getcolumn(self, name):
		return self.__dict__[name.lower()]
	def __len__(self):
		return self._length

def table_fields(dataorfn, rows=None):
	pf = None
	if isinstance(dataorfn, str):
		pf = pyfits.open(dataorfn)
		data = pf[1].data
	else:
		data = dataorfn

	colnames = data.dtype.names
	fields = tabledata()
	for c in colnames:
		col = data.field(c)
		if rows is not None:
			col = col[rows]
		fields.set(c.lower(), col)
	fields._length = len(data)
	if pf:
		pf.close()
	return fields

# ultra-brittle text table parsing.
def text_table_fields(forfn):
	f = None
	if isinstance(forfn, str):
		f = open(forfn)
		data = f.read()
		f.close()
	else:
		data = forfn.read()

	txtrows = data.split('\n')

	# column names are in the first line.
	header = txtrows.pop(0)
	header = header.split()
	assert(header[0] == '#')
	assert(len(header) > 1)
	colnames = header[1:]

	fields = tabledata()
	txtrows = [r for r in txtrows if not r.startswith('#')]
	coldata = [[] for x in colnames]
	for r in txtrows:
		cols = r.split()
		if len(cols) == 0:
			continue
		assert(len(cols) == len(colnames))
		for i,c in enumerate(cols):
			coldata[i].append(c)

	for i,col in enumerate(coldata):
		isint = True
		isfloat = True
		for x in col:
			try:
				float(x)
			except:
				isfloat = False
				isint = False
				break
			try:
				int(x)
			except:
				isint = False
				break
		if isint:
			isfloat = False

		if isint:
			vals = [int(x) for x in col]
		elif isfloat:
			vals = [float(x) for x in col]
		else:
			vals = col

		fields.set(colnames[i].lower(), array(vals))
		fields._length = len(vals)

	return fields
