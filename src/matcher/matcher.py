#!/usr/bin/python2.4
# Copyright 2010 Keir Mierle.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
__author__ = 'mierle@gmail.com (Keir Mierle)'

import sqlite3
import web
import kweb
import update_tweak

from kdb import query, Database

@kweb.get
def tweak(args):
    if not 'tweak_id' in args:
        print '404 Not found'
        return
    db = Database()
    query = lambda x: db.query(x, (args['tweak_id'],))
    result = dict(
        tweak_id=args['tweak_id'],
        catalog=query('''
            SELECT catalog_id, x, y
            FROM projected_catalog WHERE tweak_id = ?'''),
        image=query('''
            SELECT id AS image_id, x, y
            FROM image WHERE tweak_id = ?'''),
        matches=query('''
            SELECT id AS match_id, catalog_id, image_id
            FROM matches WHERE tweak_id = ?'''))
    result.update(query('''
            SELECT id as tweak_id, url, model, wcs, width, height
            FROM tweaks WHERE tweak_id = ?''')[0])
    db.close()
    return result

@kweb.post
def removematch(args):
    if (not 'tweak_id' in args or
        not 'catalog_id' in args or
        not 'image_id' in args):
        print 'Missing required parameters'
        return False
    db = Database()
    query = lambda x: db.query(x, args)
    query('''DELETE FROM matches
             WHERE tweak_id = :tweak_id
               AND catalog_id = :catalog_id''')
    query('''DELETE FROM matches
             WHERE tweak_id = :tweak_id
               AND image_id = :image_id''')
    db.close()
    return True

@kweb.post
def savematch(args):
    if not removematch(args):
        return False
    query('''INSERT INTO matches (tweak_id, catalog_id, image_id)
             VALUES (:tweak_id, :catalog_id, :image_id)''', args)

@kweb.post
def solve(args):
    print 'solve()'
    if not 'tweak_id' in args:
        print '404 Not found'
        return
    order = int(args.get('order', 1))
    print 'upd...'
    update_tweak.update_tweak(int(args['tweak_id']), order)

@kweb.get
def index(args):
    raise web.seeother('/static/matcher.html')

if __name__ == "__main__":
    kweb.app().run()
