#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys


def main():
  """Adds a photo to an album using GData Picasaweb API"""

  HELPSTRING = 'addPhoto.py --photo=photofile.{jpg,png,gif,bmp} --album=albumid [--tag=DesiredTag] [--caption="Caption for Photo"] '
  [photo,palbum,tag,caption,puser,atoken]=pwParseBasicOpts(["photo=","album=","tag=","caption="],[None,None,None,None],HELPSTRING)
  
  if photo==None:
    print "Photo File cannot be empty!"
    sys.exit(4)
  if palbum==None:
    print "albumid cannot be empty!"
    sys.exit(4)

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  albumURI='http://picasaweb.google.com/data/feed/api/user/'+pws.email+'/albumid/'+palbum
  #try:
  print "Adding photo in file %s to album %s of user %s" % (photo,palbum,pws.email)
  if(caption):
    print "...setting caption to %s" % caption
  if(tag):
    print "...tagging with tags %s" % tag

  #p=pws.InsertPhotoSimple(palbum,photo.split('/')[-1],"photo title",photo,summary=caption,keywords=tag)
  p=pws.InsertPhotoSimple(palbum,photo.split('/')[-1],"photo title",photo)
  #except:
  #  print "Error inserting photo. Make sure file exists and auth token is not expired?"

if __name__ == '__main__':
  main()
