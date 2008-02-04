#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
from time import time,ctime
import sys


def main():
  """Continuously polls for new images that have a certain tag value using GData Picasaweb API"""

  HELPSTRING = 'testPipeline.py --tag=TriggerTag'
  [tag,puser,atoken]=pwParseBasicOpts(["tag="],[None],HELPSTRING)

  if tag==None:
    print HELPSTRING
    sys.exit(4)

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  # a dictionary, keyed by entry.id.text with value being time seen
  masterDict = {}

  while True:
    allE = getAllTagEntries(tag)
    if allE:
      for e in allE:
        if pws.email in [a.email.text for a in e.author]:
          print "WARNING: tag found on image %s for user %s" % (e.id.text,pws.email)
        elif e.id.text not in masterDict:
          # get new urls as local files
          masterDict[e.id.text] = time()
          (localfilename,md5sum)=downloadEntry(e,verbose=True)
          thisalbumname=makeAlbumnameFromMD5(md5sum)
          thisphotouser=e.id.text[e.id.text.find('/user/')+6:].split('/')[0]
          thiscaptiontext="%s from %s" % (e.content.src.split('/')[-1],thisphotouser)
          thisalbumentry=insertAlbumNonDuplicate(pws.email,thisalbumname,verbose=True)
          thispalbum=thisalbumentry.gphoto_id.text
          thistag="SkyPhotoLocator:User:"+thisphotouser
          thiscomment="Copied from "+e.GetHtmlLink().href+\
			   " on "+ctime()+" with md5sum="+md5sum
          ##p=uploadPhoto(localfilename,thispalbum,caption=thiscaptiontext,tag=thistag,verbose=True)
          thisphotoentry=uploadPhoto(localfilename,thispalbum,caption=thiscaptiontext,verbose=True)
          insertTag(thistag,thisphotoentry.gphoto_id.text,thispalbum)
          insertComment(thiscomment,thisphotoentry.gphoto_id.text,thispalbum,verbose=True)
    else:
      print "No images found with tag="+tag

if __name__ == '__main__':
  main()


