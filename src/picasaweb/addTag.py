#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys


def main():
  """Adds a tag value to a photo using GData Picasaweb API"""

  HELPSTRING = 'addTag.py --tag=DesiredTag'
  [tag,pphotoid,palbum,puser,atoken]=pwParsePhotoOpts(["tag="],[None],HELPSTRING)
  if tag==None:
    print "Tag cannot be empty!"
    sys.exit(4)

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  if puser==None:
    puser=pws.email
  photoURI='http://picasaweb.google.com/data/feed/api/user/'+puser+'/albumid/'+palbum+'/photoid/'+pphotoid
  # CHECK IF TAG IS ALREADY THERE?
  try:
    pws.InsertTag(photoURI,tag)
  except:
    print "Error inserting tag. Make sure auth token is not expired?"

if __name__ == '__main__':
  main()
