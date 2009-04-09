import os
import os.path
import sqlite
import cPickle as pickle

# Returns a list of dictionaries.
def summarize_dir(dirname):
	lst = []
	print 'summarize_dir', dirname
	dirlist = os.listdir(dirname)
	dirlist.sort()
	for p in dirlist:
		path = os.path.join(dirname, p)
		#print 'path', path
		if os.path.isdir(path):
			#print 'dir: recurse'
			lst += summarize_dir(path)
			continue
		if not os.path.basename(path) == 'jobdata.db':
			continue
		#print 'reading', path

		# HACK - job id isn't in the jobdata.db!
		jobdir = os.path.dirname(path)
		pth = jobdir
		idnum = os.path.basename(pth)
		pth = os.path.dirname(pth)
		month = os.path.basename(pth)
		pth = os.path.dirname(pth)
		site = os.path.basename(pth)

		#print 'site', site, 'month', month, 'id', idnum
		jobid = '%s-%s-%s' % (site, month, idnum)

		try:
			conn = sqlite.connect(path)
			q = conn.db.execute('select * from jobdata')
			q.row_list.append(('jobid', jobid))
			q.row_list.append(('jobdir', jobdir))
			d = dict(q.row_list)
			#print 'dict', d
			lst.append(d)
		except:
			print 'Failed to read database', path
	return lst


if __name__ == '__main__':

	#out = '200904.pickle'
	out = 'alpha.pickle'
	if not os.path.exists(out):
		#basedir = '/home/gmaps/ontheweb-data/alpha/200904'
		basedir = '/home/gmaps/ontheweb-data/alpha'
		lst = summarize_dir(basedir)
		print 'Pickling...'
		f = open(out, 'wb')
		pickle.dump(lst, f)
		f.close()
	else:
		#print 'Reading pickle', out
		f = open(out)
		lst = pickle.load(f)
		f.close()

	for d in lst:
		#if d['email'] != 'christopher@stumm.ca':
		#	continue
		jobdir = d['jobdir']
		wcsfn = os.path.join(jobdir, 'wcs.fits')
		if not os.path.exists(wcsfn):
			continue

		print wcsfn

		#print d.keys()

		#if 'imgurl' in d:
		#	print d['imgurl']

		#print wcsfn.replace('/home/gmaps/ontheweb-data/', 'wcsfn ')

		#print d['jobid']

