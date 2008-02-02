#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys


def main():
  """Set an image caption"""

  HELPSTRING = 'setImageCaption.py --caption="Desired Caption Text"'
  [caption,pphotoid,palbum,puser,atoken]=pwParsePhotoOpts(["caption="],[None],HELPSTRING)
  if caption==None:
    print "Caption cannot be empty!"
    sys.exit(4)

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  e=getPhotoEntry(palbum,pphotoid,puser=puser)
  if e:
    e.summary.text=caption
    try:
      pws.UpdatePhotoMetadata(e)
    except:
      print "Could not update photo metadata."


if __name__ == '__main__':
  main()
