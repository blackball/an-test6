#!/usr/bin/python2.4

import kdb
import numpy
import run_command

db = kdb.Database()
query = kdb.query

image = 1000*numpy.random.rand(100, 2)
catalog = image + 2
tweak_id = 42

def update_tweak(tweak_id):
    db = kdb.Database()
    query = lambda x: db.query(x, (tweak_id,))

    lines = []

    query('BEGIN TRANSACTION');

    for row in query('''
            SELECT image.x AS x, image.y AS y, catalog.a AS a, catalog.d AS d
            FROM matches
                 INNER JOIN image ON image.id = matches.image_id
                 INNER JOIN catalog ON catlog.image_id = matches.image_id
            WHERE matches.tweak_id = ?'''):
        lines.append('%(x)s %(y)s %(a)s %(d)s' % row)

    catalog_ids = []
    for row in query('''
            SELECT catalog_id, a, d
            FROM catalog WHERE tweak_id = ?''')
        lines.append('-1 -1 %(a)s %(d)s' % row)
        catalog_ids.append(row['catalog_id'])

    # -W -H
    # -X and -Y for boresight
    # match:   x y a d
    # catalog: -1 -1 a d
    cmd = 'keir_thing -W %(width)s -H %(height)s' % query('''
           SELECT id as tweak_id, url, model, wcs, width, height
           FROM tweaks WHERE tweak_id = ?''')[0]
    exitcode, out, err = run_command.run_command(
        cmd, stdindata='\n'.join(lines))

    # Update the catalog positions.
    xys = out.split('\n')
    assert len(xys) == len(catalog_ids)
    for catalog_id, xy in zip(xys, catalog_ids):
        x, y = map(float, xy.split())
        query('''INSERT INTO projected_catalog (catalog_id, x, y)
                 VALUES (?, ?, ?)''', (catalog_id, x, y))

    query('COMMIT');
    db.close()
#!/usr/bin/python2.4

import kdb
import numpy
import run_command

db = kdb.Database()
query = kdb.query

image = 1000*numpy.random.rand(100, 2)
catalog = image + 2
tweak_id = 42

def update_tweak(tweak_id):
    db = kdb.Database()
    query = lambda x: db.query(x, (tweak_id,))

    lines = []

    query('BEGIN TRANSACTION');

    for row in query('''
            SELECT image.x AS x, image.y AS y, catalog.a AS a, catalog.d AS d
            FROM matches
                 INNER JOIN image ON image.id = matches.image_id
                 INNER JOIN catalog ON catlog.image_id = matches.image_id
            WHERE matches.tweak_id = ?'''):
        lines.append('%(x)s %(y)s %(a)s %(d)s' % row)

    catalog_ids = []
    for row in query('''
            SELECT catalog_id, a, d
            FROM catalog WHERE tweak_id = ?''')
        lines.append('-1 -1 %(a)s %(d)s' % row)
        catalog_ids.append(row['catalog_id'])

    # -W -H
    # -X and -Y for boresight
    # match:   x y a d
    # catalog: -1 -1 a d
    cmd = 'keir_thing -W %(width)s -H %(height)s' % query('''
           SELECT id as tweak_id, url, model, wcs, width, height
           FROM tweaks WHERE tweak_id = ?''')[0]
    exitcode, out, err = run_command.run_command(
        cmd, stdindata='\n'.join(lines))

    # Update the catalog positions.
    xys = out.split('\n')
    assert len(xys) == len(catalog_ids)
    for catalog_id, xy in zip(xys, catalog_ids):
        x, y = map(float, xy.split())
        query('''INSERT INTO projected_catalog (catalog_id, x, y)
                 VALUES (?, ?, ?)''', (catalog_id, x, y))

    query('COMMIT');
    db.close()
