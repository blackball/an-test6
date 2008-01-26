#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys


def main():
  """Adds a photo to an album using GData Picasaweb API"""

  HELPSTRING = 'addPhoto.py --photo=photo.{jpg,png,gif,bmp} [--tag=DesiredTag] [--caption="Caption for Photo"] '
  [tag,pphotoid,puser,palbum,gdata_authtoken]=pwParsePhotoOpts(["photo="],[''],HELPSTRING)
  print [tag,pphotoid,puser,palbum,gdata_authtoken]
  
  if tag=='':
    print "Tag cannot be empty!"
    sys.exit(4)

  pws = pwInit()
  pwAuth(pws,gdata_authtoken)

  photoURI='http://picasaweb.google.com/data/feed/api/user/'+puser+'/albumid/'+palbum+'/photoid/'+pphotoid
  try:
    pws.InsertTag(photoURI,tag)
  except:
    print "Error inserting tag. Make sure auth token is not expired?"

if __name__ == '__main__':
  main()
