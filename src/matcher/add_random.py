#!/usr/bin/python2.4

import kdb
import numpy

db = kdb.Database()
query = kdb.query

image = 1000*numpy.random.rand(100, 2)
catalog = image + 2
tweak_id = 42

for x, y in image:
  query('''INSERT INTO image (tweak_id, x, y)
           VALUES (?, ?, ?)''', (tweak_id, x, y))

for x, y in catalog:
  query('''INSERT INTO projected_catalog (tweak_id, x, y)
           VALUES (?, ?, ?)''', (tweak_id, x, y))
  # These ra/dec are bogus, obviously
  query('''INSERT INTO catalog (tweak_id, a, d)
           VALUES (?, ?, ?)''', (tweak_id, x, y))

query('''INSERT INTO tweaks (id, url, model, wcs, width, height)
         VALUES (?, 'http://fake', 'TAN', '', 800, 600)''',
      (tweak_id,))

db.close()
