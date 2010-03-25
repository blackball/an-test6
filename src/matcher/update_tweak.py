#!/usr/bin/python2.4

import kdb
from astrometry.util import run_command

db = kdb.Database()
query = db.query

def update_tweak(tweak_id):
    print 'hello world, i am update_tweak'
    db = kdb.Database()
    query = db.query

    lines = []

    query('BEGIN TRANSACTION');

    print 'here 1'
    for row in query('''
            SELECT image.x AS x, image.y AS y, catalog.a AS a, catalog.d AS d
            FROM matches
                 INNER JOIN image ON image.id = matches.image_id
                 INNER JOIN catalog ON catalog.id = matches.catalog_id
            WHERE matches.tweak_id = ?''', (tweak_id,)):
        lines.append('%(x)s %(y)s %(a)s %(d)s' % row)

    print 'here 2'
    catalog_ids = []
    for row in query('''
            SELECT id, a, d
            FROM catalog WHERE tweak_id = ?''', (tweak_id,)):
        lines.append('-1 -1 %(a)s %(d)s' % row)
        catalog_ids.append(row['id'])

    print 'lines:', '\n'.join(lines)
        
    # -W -H
    # -X and -Y for boresight
    # match:   x y a d
    # catalog: -1 -1 a d
    cmd = 'keirthing -W %(width)s -H %(height)s' % query('''
           SELECT id as tweak_id, url, model, wcs, width, height
           FROM tweaks WHERE tweak_id = ?''', (tweak_id,))[0]
    exitcode, out, err = run_command.run_command(
        cmd, stdindata='\n'.join(lines))

    print 'exitcode', exitcode
    print 'out \n>>%s<<\n' % out
    print 'err \n>>%s<<\n' % err

    # Update the catalog positions.
    xys = out.split('\n')[:-1]
    print 'xys', len(xys), 'cat', len(catalog_ids)
    assert len(xys) == len(catalog_ids)
    for xy, catalog_id in zip(xys, catalog_ids):
        x, y = map(float, xy.split())
        query('''INSERT INTO projected_catalog (tweak_id, catalog_id, x, y)
                 VALUES (?, ?, ?, ?)''', (tweak_id, catalog_id, x, y))

    query('COMMIT');
    db.close()
