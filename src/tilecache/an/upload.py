#!/usr/bin/env python

#Copyright (c) 2006, Lindsey Simon <lsimon@finetooth.com>
#All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are
#met:
#
#       Redistributions of source code must retain the above copyright
#notice, this list of conditions and the following disclaimer.
#       Redistributions in binary form must reproduce the above
#copyright notice, this list of conditions and the following disclaimer
#in the documentation and/or other materials provided with the
#distribution.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
#TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
#PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
#OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import time
import random, re
import sys
import cgi
import pyclamav
import cgitb

def mkSessionDir(fileupload_sessionid):
   fileupload_session_dir = '/tmp/htmlform_fileupload-%s' % fileupload_sessionid
   try: os.mkdir(fileupload_session_dir)
   except: pass
      #print # end headers
      #print "ERROR: PB upload session already exists."
      #sys.exit(-1)

   return fileupload_session_dir
   
def mkFormfieldKeyDir(fileupload_session_dir, formfield_key):
   formfield_key_dir = '%s/%s' % (fileupload_session_dir, formfield_key)
   try: os.mkdir(formfield_key_dir)
   except: pass
   return formfield_key_dir

def writeMeta(formfield_key_dir):
   f = open(formfield_key_dir + '/meta_data', 'w')
   f.write('starttime=' + str(int(time.time())) + '\n') # timestamp
   f.write('totalsize=' + os.environ['CONTENT_LENGTH'] + '\n') # total size of data being uploaded
   f.write('pid=' + str(os.getpid())) # pid needed to cancel
   f.close()

def readStdin(formfield_key_dir):
   f = open(formfield_key_dir + '/cgi_data', 'w+')
   parsedFilename = False
   parsedMimetype = False
   while True:
      data = sys.stdin.read(1024*64) # read 64k at a time
      if not data: break
      if not parsedFilename:
         match = re.search("filename=\"([^\"]+)\"", data)
         if match:
            filename = match.group(1)
            x = open(formfield_key_dir + '/meta_data', 'a')
            x.write('\nfilename=\"' + filename + "\"")
            x.close()
            parsedFilename = True
      if not parsedMimetype:
         match = re.search("Content-Type:\s(\S+)", data)
         if match:
            mimetype = match.group(1)
            x = open(formfield_key_dir + '/meta_data', 'a')
            x.write('\nmimetype=' + mimetype)
            x.close()
            parsedMimetype = True
            
      f.write(data)
      f.flush() # so the ajax poller gets an accurate reading
      time.sleep(0.35)
   f.seek(0)
   return f


   
def writeFileData(formfield_key_dir, form):

   # This assumes we're only sending one file per cgi, which is how we've made things work         
   for name in form:
      field = form[name]
      
      if field.filename:
         # path to the file
         fpath = '%s/%s' % (formfield_key_dir, field.name)
         
         # write out the file data into the file and make a pointer so we can check it for virii
         open(fpath, 'w').write(field.file.read())
         
         # now get the mimetype from the file command instead of what the browser says
         cmd = 'file -bi %s' % fpath
         mimetype = os.popen(cmd).read()[:-1]
         
         # check that file for virii
         hasVirus,virusName = list(pyclamav.scanfile(fpath))
         
         
         # open a file to hold the form input data minus the files
         f = open('%s/%s' % (formfield_key_dir, 'form_data'), 'w')
         
         # write out properties 
         f.write("htmlform_fileupload_name=%s\n" % field.name)
         f.write("htmlform_fileupload_filename=\"%s\"\n" % field.filename)
         f.write("htmlform_fileupload_type=%s\n" % mimetype)
         f.write("htmlform_fileupload_hasVirus=%s\n" % hasVirus)
         f.write("htmlform_fileupload_virusName=%s\n" % virusName)
         
         f.close()
         break


      
#########
# START #
#########

cgitb.enable()

# send headers so that the browser doesn't freak out
print "Content-Type: text/html"
print


# get some values from the query string
# the query string has values in QUERY_STRING even though it's a POST
# this is a dirty little hack - we have to set the action in the form like so:
# HTMLForm_fileupload.cgi?fileupload_sessionid=val&formfield_key=val
fileupload_sessionid = cgi.parse_qs(os.environ['QUERY_STRING'])['fileupload_sessionid'][0]
formfield_key = cgi.parse_qs(os.environ['QUERY_STRING'])['formfield_key'][0]

# make a dir in tmp for upload session
fileupload_session_dir = mkSessionDir(fileupload_sessionid)
formfield_key_dir = mkFormfieldKeyDir(fileupload_session_dir, formfield_key)


# write the meta data
writeMeta(formfield_key_dir)

# read in data over stdin and write to a file as the file uploads
f = readStdin(formfield_key_dir)

# use the file written to above to create a form object
form = cgi.FieldStorage(f)
f.close()

# now write the form data to a file 
writeFileData(formfield_key_dir, form)

# output a lil green checkbox
img = "<img src='mimetypeicons/green_checkbox.gif' alt='File Upload Complete' height='16' width='16' /> \n"
print img


