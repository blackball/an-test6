#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
import sys



def main():
  """Add a comment to an image"""

  HELPSTRING = 'addComment.py --comment "Desired Comment Text"'
  [comment,pphotoid,puser,palbum,gdata_authtoken]=pwParsePhotoOpts(["comment="],[''],HELPSTRING)
  if comment=='':
    print "Comment cannot be empty!"
    sys.exit(4)
  pws = pwInit()
  pwAuth(pws,gdata_authtoken)


  e=GetPhotoEntry(pws,puser,palbum,pphotoid)
  if e:
    print e.commentingEnabled.text
    print e.commentCount.text
    try:
      print comment
      pws.InsertComment(e,comment)
    except:
      print "Error inserting comment. Make sure auth token is not expired?"

  #photoURI='http://picasaweb.google.com/data/feed/api/user/'+puser+'/albumid/'+palbum+'/photoid/'+pphotoid+"?kind=comment"
  #try:
  #  pws.InsertComment(photoURI,comment)
  #except:
  #  print "Error inserting comment. Make sure auth token is not expired?"

if __name__ == '__main__':
  main()
