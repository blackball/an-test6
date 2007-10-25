#!/usr/bin/env python

# This file was imported into the Astrometry.net suite and *very* heavily
# modified.  The original copyright is below.  Our modifications are
# Copyright 2007 Dustin Lang and licensed under the terms of the GPL
# version 2.


# Copyright (c) 2006, Lindsey Simon <lsimon@finetooth.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#       Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#       Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import time
import random
import re
import sys
import cgi
import cgitb

import multipart

def log(x):
	print x

class Upload(multipart.FileMultipart):
   basedir = None
   id_field = None

   id = None
   filename = None
   progress = None
   starttime = None
   contentlength = 0
   predictedsize = 0
   bytes_written = 0

   def __init__(self, fin=sys.stdin):
      super(Upload, self).__init__(fin)
      self.starttime = int(time.time())

   def set_basedir(self, dir):
      self.basedir = dir

   def set_id_field(self, field):
      self.id_field = field

   def add_part(self, part):
      super(Upload, self).add_part(part)
      key = 'field'
      datakey = 'data'
      if key in part and datakey in part and part[key] == self.id_field:
         id = part[datakey]
         idre = re.compile('^[A-Za-z0-9]+$')
         if not idre.match(id):
            log('Invalid id "%s"' % id)
            self.abort()
         self.set_id(id)
         self.write_progress()
         # We will have read the Content-Length header by now...
         key = 'Content-Length'
         if not key in self.headers:
            log('Content-Length unknown.')
            self.abort()
         self.contentlength = int(self.headers[key])

   def abort(self):
      sys.exit(-1)

   def set_id(self, id):
      log('Setting ID to %s' % id)
      self.id = id
      self.filename = self.basedir + '/' + self.id
      self.progress = self.filename + '.progress'
      if (os.path.exists(self.progress) or os.path.exists(self.filename)):
         log('Upload ID "%s" already exists.' % id)
         self.abort()

   # Computes the filename to write the data of this "part body" to.
   def get_filename(self, field, filename, currentpart):
      if not self.id:
         log('Upload ID is not known.')
         return None
      if not self.filename:
         log('Filename is not known.')
         return None
      #log('Offset of header: %d' % self.body_offset)
      #log('Offset now: %d' % self.bytes_parsed)
      #log('Bytes since header: %d' % (self.bytes_parsed - self.body_offset))
      self.predictedsize = self.contentlength \
                           - (self.bytes_parsed - self.body_offset) \
                           - (len(self.boundary) + 6)
      # magic number 6: the ending boundary token is CRLF "--" BOUNDARY CRLF
      log('Predicted file size: %d' % self.predictedsize)
      return self.filename

   def set_bytes_written(self, nb):
      super(Upload, self).set_bytes_written(nb)
      self.bytes_written = nb

   def write_progress(self):
      if not self.progress:
         return
      f = open(self.progress, 'w')
      f.write('starttime=%d\n' % self.starttime)
      if self.predictedsize:
         sz = self.predictedsize
      else:
         sz = self.contentlength
      f.write('totalsize=%d\n' % sz)
      f.write('pid=%d\n' % os.getpid())
      now = int(time.time())
      f.write('timenow=%d\n' % now)
      f.write('written=%d\n' % self.bytes_written)
      dt = now - self.starttime
      f.write('elapsed=%d\n' % dt)
      if self.bytes_written and dt>0:
         avgspeed = self.bytes_written / float(dt)
         f.write('speed=%f\n' % avgspeed)
         left = sz - self.bytes_written
         if left < 0:
            left = 0
         eta = left / avgspeed
         f.write('eta=%d\n' % int(eta))
      f.close()



#########
# main  #
#########

#cgitb.enable()

# send headers so that the browser doesn't freak out
print "Content-Type: text/html"
print

try:
   upload_base_dir = os.environ['UPLOAD_DIR']
except KeyError:
   upload_base_dir = '/tmp'
try: 
   upload_id_field = os.environ['UPLOAD_ID_STRING']
except KeyError:
   upload_id_field = 'upload_id'

up = Upload()
#up.blocksize = 1
up.set_basedir(upload_base_dir)
up.set_id_field(upload_id_field)
#while up.readmore():
#   up.write_progress()
while True:
   print 'reading...'
   ok = up.readmore()
   if not ok:
      break
   print 'writing progress...'
   sys.stdout.flush()
   up.write_progress()
if up.error:
   print 'Parser failed.'
   up.abort()
else:
   print 'Parser succeeded'
   print 'Message headers:'
   for k,v in up.headers.items():
      print '  ', k, '=', v
   for p in up.parts:
      print '	 Part:'
      for k,v in p.items():
         print '	   ', k, '=', v

print 'Upload complete.'
