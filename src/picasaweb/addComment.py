#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys


def main():
  """Adds a comment to a photo using GData Picasaweb API"""

  HELPSTRING = 'addComment.py --comment "Desired Comment Text"'
  [comment,pphotoid,palbum,puser,atoken]=pwParsePhotoOpts(["comment="],[None],HELPSTRING)

  if comment==None:
    print "Comment cannot be empty!"
    print HELPSTRING
    sys.exit(4)

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  insertComment(comment,pphotoid,palbum,puser=puser)


if __name__ == '__main__':
  main()
