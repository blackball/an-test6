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

DATABASE = 'matcher.sqlite'

class Database(object):
  def __init__(self):
    def dict_factory(cursor, row):
      d = {}
      for idx, col in enumerate(cursor.description):
        d[col[0]] = row[idx]
      return d

    self.conn = sqlite3.connect(DATABASE, isolation_level=None)
    self.conn.row_factory = dict_factory
    self.c = self.conn.cursor()

  def query(self, sql, params=None):
    print sql
    if params is None:
      return [r for r in self.c.execute(sql)]
    print '  where ? ==', params
    return [r for r in self.c.execute(sql, params)]

  def close(self):
    self.c.close()
    self.conn.commit()
    self.conn.close()

def query(sql, params=None):
    db = Database()
    result = db.query(sql, params)
    db.close()
    return result
