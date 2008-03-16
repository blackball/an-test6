#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
from time import time
import sys


def main():
  """Continuously polls for new images that have a certain tag value using GData Picasaweb API"""

  DEFAULT_TAG = "SkyPhotoLocatorRequest"
  DEFAULT_USER = "astrometrynet"

  HELPSTRING = 'testPipeline.py --tag=TriggerTag'
  [tag,puser,atoken]=pwParseBasicOpts(["tag="],[None],HELPSTRING)

  if tag is None:
    tag=DEFAULT_TAG

  pws = pwInit()
  pwAuth(gdata_authtoken=atoken,gdata_user=puser)

  if pws.email != DEFAULT_USER:
    print "WARNING -- for now, I advise you only run with service user=%s not %s" % (DEFAULT_USER,pws.email)
    sys.exit(5)

  if tag != DEFAULT_TAG:
    print "WARNING -- for now, I advise you only run with TriggerTag=%s not %s" % (DEFAULT_TAG,tag)
    sys.exit(5)

  # a dictionary, keyed by entry.GetHtmlLink().href value=md5sum
  masterDict = {}

  # see what photos we already have in the gallery (WARNING -- max 1000, will prob have to poll by album eventually)
  currentPhotos=pws.GetFeed(makePhotoFeed(pws.email,None))

  # list photos and fill in master dict
  if currentPhotos and currentPhotos.entry:
    print "Photos currently loaded in service account %s:" % pws.email
    for e in currentPhotos.entry:
      thisc=pws.GetFeed(e.GetCommentsUri()).entry[0].content.text
      thislink=thisc[12:thisc.find(' on ')]
      thismd5=getMD5SumFromComment(thisc)
      masterDict[thislink]=thismd5
      print "  %s (md5sum=%s)" % (thislink,thismd5)

  # now go poll for new photos with this tag
  
  while True:
    
    allE = getAllTagEntries(tag)
    if allE:
      for e in allE:
        thislink=e.GetHtmlLink().href
        if pws.email in [a.email.text for a in e.author]:
          print "WARNING: tag found on image %s for user %s" % (e.id.text,pws.email)
        elif thislink not in masterDict:
          # A NEW PHOTO!
          username=getUsername(e)
          
          # comment that we saw the tag
          insertComment(makeSawTagCommentForUser(username,tag),\
                        pentry=e,userid=username,verbose=True)
          
          # get new urls as local files
          (localfilename,md5sum,localsize)=downloadEntry(e,verbose=True)

          # comment that we downloaded
          insertComment(makeDownloadedCommentForUser(username),\
                        pentry=e,userid=username,verbose=True)

          # TRY TO SOLVE HERE
          # for now, just stub out always true
          solved=True

          if solved:
            
            # add an key to the master dict
            masterDict[thislink] = md5sum

            # figure out what album this photo goes into based on its md5sum, create if necessary
            eAlbum=insertAlbumNonDuplicate(pws.email,makeAlbumnameFromMD5(md5sum),verbose=True)
            albumname=eAlbum.name.text
          
            eNew=uploadPhoto(localfilename,albumname,caption=makeCaptionForRobot(e),verbose=True)
            insertTag(makeUserTagForRobot(username),eNew.gphoto_id.text,albumname)
            insertComment(makeCopiedCommentForRobot(e,localsize,md5sum,ctime()),\
                          pentry=eNew,verbose=True)

            # comment that the photo is now on the gallery
            insertComment(makeSolvedCommentForUser(username,makeSimpleURL(eNew,albumname=albumname)),\
                          pentry=e,userid=username,verbose=True)

          else:

            # comment that we couldn't solve the photo
            insertComment(makeFailedCommentForUser(username),\
                          pentry=e,userid=username,verbose=True)

          
        #elif thislink in masterDict:
        #  print "  skipping photo %s (already loaded)" % thislink

        
    else:
      
      print "No images found with tag="+tag




if __name__ == '__main__':
  main()


