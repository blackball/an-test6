#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *

import getopt
import sys
import pickle
#import string
#import time

# login name at picasaweb of the robot who will be leaving comments as to progress
SERVICEUSER = "astrometrydotnet"

HELPSTRING = 'getAllTagged.py --tag DesiredTag --commentUser userid'

pws = gdata.photos.service.PhotosService()
#pws.ClientLogin(username, password)

# to update caption, change the "summary" field of
# the PhotoEntry object you get from ?kind=photo uri
# and then call pws.UpdatePhotoMetadata

 
def main():
  """Gets all images that have a certain tag value using GData Picasaweb API"""

  # parse command line options
  try:
    opts, args = getopt.getopt(sys.argv[1:], "", ["tag=","commentUser="])
  except getopt.error, msg:
    print HELPSTRING
    sys.exit(2)

  tag = ''
  commentUser = SERVICEUSER.lower()

  # Process options
  for o, a in opts:
    if o == "--tag":
      tag = a.replace(' ','%20')
    if o == "--commentUser":
      commentUser = a.lower()
  if tag=='':
    print HELPSTRING
    sys.exit(2)

  allE = getAllTagEntries(tag,pws)
  if(allE==None):
    sys.exit(3)

  # a dictionary, keyed by entry.id.text with value being a list: [ [commentlist] ]
  masterDict = {}
  
  # get all comments on each image in the entrylist
  for e in allE:
    masterDict[e.id.text] = [[]]
    c = pws.GetFeed(e.GetCommentsUri())
    print "found %d comments for image %s" % (len(c.entry),e.id.text)
    for ce in c.entry:
      for cauthor in ce.author:
        if cauthor.user.text.lower()==commentUser:
          masterDict[e.id.text][0].append(ce.content.text)

  for candidate in masterDict:
    for c in masterDict[candidate][0]:
      print candidate+"; comment="+c
  

  
  
  #albumFeedURIs = getUniqueAlbumFeedURIs(allE)
  #albumFeeds = []
  #for a in albumFeedURIs:
  #  ff=pws.GetFeed(a)
  #  albumFeeds.append(ff)
    #NumCommentsWithTag = 0
    #for c in ff.entry:
    #  thisCommentAuthor = c.author[0].user.text.lower()
    #  if(thisCommentAuthor==commentUser):
    #    thisCommentID = c.id.text
    #    thisID = thisCommentID[0:thisCommentID.rfind('/commentid/')]
    #    if thisID in allIDs:
    #      allIDs[thisID].append(c)
    #      NumCommentsWithTag += 1
    #if(NumCommentsWithTag>0):
    #  print "%s <%s> has %d/%d tagged and %d/%d commented" % \
    #        (ff.user.text,ff.title.text,albumFeedURIs[a],int(ff.numphotos.text),\
    #         NumCommentsWithTag,len(ff.entry))

    
    
  #for e in allE:
  #  numComments = getPhotoComments(e)
  #  if(numComments>0):
  #    print "C: %d URL:%s" % (numComments,getPhotoPageHref(e))


  #print "Checking for comments by user "+commentUser


    


if __name__ == '__main__':
  main()
