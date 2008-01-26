#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys


def main():
  """Set an image caption"""

  HELPSTRING = 'setImageCaption.py --caption="Desired Caption Text"'
  [caption,pphotoid,puser,palbum,gdata_authtoken]=pwParsePhotoOpts(["caption="],[''],HELPSTRING)
  if caption=='':
    print "Caption cannot be empty!"
    sys.exit(4)

  pws = pwInit()
  pwAuth(pws,gdata_authtoken)

  e=GetPhotoEntry(pws,puser,palbum,pphotoid)
  if e:
    e.summary.text=caption
    try:
      pws.UpdatePhotoMetadata(e)
    except:
      print "Could not update photo metadata."


if __name__ == '__main__':
  main()
