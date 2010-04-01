#!/usr/bin/env python

import os
import sys

from astrometry.util.pyfits_utils import *
import pyfits

import kdb
import numpy

def lastid(db):
	print db.query('SELECT last_insert_rowid()')
	return int(db.query('SELECT last_insert_rowid() AS id')[0]['id'])

sample_ul = 'http://oven.cosmo.fas.nyu.edu/blind/status.php?job=tor-200704-39947589&get=index.rd.fits'

def wget(url, fn):
	os.system("wget '%s' -O %s" % (url, fn))

if __name__ == '__main__':
	args = sys.argv[1:]
	if not len(args):
		print 'Usage: %s <base filename> ...' % sys.argv[0]
		sys.exit(-1)

	db = kdb.Database()
	query = db.query
	query('BEGIN TRANSACTION')

	for base in args:
                # you need corrsepondences.fits,
                # field.xy.fits (rename to .axy); index.xy.fits (rename to -indx.xyls); index.rd.fits (rename to .rdls)
		url = ''
		if base.startswith('http'):
			print 'Downloading from:', base
			xyfn = 'field.xy.fits'
			rdfn = 'index.rd.fits'
			ixyfn = 'index.xy.fits'
			corrfn = 'correspondences.fits'
			url = base + '&get=fullsize.png'
			for f in xyfn, rdfn, ixyfn, corrfn:
				wget(base + '&get=' + f, f)
		else:
			print 'Reading input files with base filename', base
			xyfn = '%s.axy' % base
			rdfn = '%s.rdls' % base
			ixyfn = '%s-indx.xyls' % base
			corrfn = '%s.corr' % base

		xy = table_fields(xyfn)
		rd = table_fields(rdfn)
		ixy = table_fields(ixyfn)
		corr = table_fields(corrfn)

		hdr = pyfits.open(xyfn)[1].header
		W = hdr['IMAGEW']
		H = hdr['IMAGEH']

		query('''INSERT INTO tweaks (url, model, wcs, width, height)
		VALUES (?,?,?,?,?)''', (url, 'TAN', '', W, H))
		tweak_id = lastid(db)
		print 'tweak_id', tweak_id

		imgids = {}
		for i,(x,y) in enumerate(zip(xy.x, xy.y)):
			query('''INSERT INTO image (tweak_id, x, y)
			VALUES (?, ?, ?)''', (tweak_id, float(x), float(y)))
			imgids[i] = lastid(db)
			print 'imgid', imgids[i]

		catids = {}
		for i,(r,d) in enumerate(zip(rd.ra, rd.dec)):
			query('''INSERT INTO catalog (tweak_id, a, d)
			VALUES (?, ?, ?)''', (tweak_id, float(r), float(d)))
			catids[i] = lastid(db)
			print 'catid', catids[i]

		for i,(x,y) in enumerate(zip(ixy.x, ixy.y)):
			query('''INSERT INTO projected_catalog (tweak_id, catalog_id, x, y)
			VALUES (?, ?, ?, ?)''', (tweak_id, catids[i], float(x), float(y)))

		for iimg, icat in zip(corr.field_id, corr.index_id):
			query('''INSERT INTO matches (tweak_id, catalog_id, image_id)
			VALUES (?, ?, ?)''', (tweak_id, int(catids[icat]), int(imgids[iimg])))
	query('COMMIT')

	db.close()
