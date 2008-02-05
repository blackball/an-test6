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

  # a dictionary, keyed by entry.GetHtmlLink().href value=md5smu
  masterDict = {}

  currentPhotos=pws.GetFeed("http://picasaweb.google.com/data/feed/api/user/"+pws.email+"?kind=photo")
  if currentPhotos and currentPhotos.entry:
    print "Photos currently loaded in service account %s:" % pws.email
    for e in currentPhotos.entry:
      thisc=pws.GetFeed(e.GetCommentsUri()).entry[0].content.text
      thislink=thisc[12:thisc.find(' on ')]
      thismd5=thisc[thisc.find('md5sum=')+7:]
      masterDict[thislink]=thismd5
      print "  %s (md5sum=%s)" % (thislink,thismd5)

  while True:
    allE = getAllTagEntries(tag)
    if allE:
      for e in allE:
        thislink=e.GetHtmlLink().href
        if pws.email in [a.email.text for a in e.author]:
          print "WARNING: tag found on image %s for user %s" % (e.id.text,pws.email)
        elif thislink not in masterDict:
          # get new urls as local files
          (localfilename,md5sum)=downloadEntry(e,verbose=True)
          masterDict[thislink] = md5sum
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
        #elif thislink in masterDict:
        #  print "  skipping photo %s (already loaded)" % thislink
    else:
      print "No images found with tag="+tag

if __name__ == '__main__':
  main()


