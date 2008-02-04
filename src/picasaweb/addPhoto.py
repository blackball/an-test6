#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys


def main():
  """Adds a photo to an album using GData Picasaweb API"""

  HELPSTRING = 'addPhoto.py --photofile=file_to_upload.{jpg,png,gif,bmp} --albumid=albumid [--tag=DesiredTag] [--caption="Caption for Photo"] '
  [photofile,palbum,tag,caption,puser,atoken]=pwParseBasicOpts(["photofile=","albumid=","tag=","caption="],[None,None,None,None],HELPSTRING)
  
  if photofile==None or palbum==None:
    print HELPSTRING
    sys.exit(4)

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  uploadPhoto(photofile,palbum,tag=tag,caption=caption,verbose=True)

if __name__ == '__main__':
  main()
