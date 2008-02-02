#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys



def main():
  """Add a comment to an image"""

  HELPSTRING = 'addComment.py --comment "Desired Comment Text"'
  [comment,pphotoid,palbum,puser,atoken]=pwParsePhotoOpts(["comment="],[None],HELPSTRING)
  if comment==None:
    print "Comment cannot be empty!"
    sys.exit(4)
  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)


  e=getPhotoEntry(palbum,pphotoid,puser=puser)
  if e:
    try:
      pws.InsertComment(e,comment)
    except:
      print "Error inserting comment. Make sure auth token is not expired?"


if __name__ == '__main__':
  main()
