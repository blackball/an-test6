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
    print HELPSTRING
    sys.exit(4)

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  insertTag(tag,pphotoid,palbum)


if __name__ == '__main__':
  main()
