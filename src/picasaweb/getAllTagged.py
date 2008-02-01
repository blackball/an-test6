#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *

import getopt
import sys





def main():
  """Gets all images that have a certain tag value using GData Picasaweb API"""

  # login name at picasaweb of the robot who will be leaving comments as to progress
  SERVICEUSER = "astrometrydotnet"


  HELPSTRING = 'getAllTagged.py --tag=DesiredTag [--serviceuser=userid (default '+SERVICEUSER.lower()+')]'
  # parse command line options
  try:
    opts, args = getopt.getopt(sys.argv[1:], "", ["tag=","serviceuser="])
  except getopt.error, msg:
    print HELPSTRING
    sys.exit(2)

  tag = None
  serviceuser = SERVICEUSER.lower()

  # Process options
  for o, a in opts:
    if o == "--tag":
      tag = a.replace(' ','%20')
    if o == "--serviceuser":
      serviceuser = a.lower()
  if tag==None:
    print HELPSTRING
    sys.exit(2)


  pws=pwInit()

  allE = getAllTagEntries(tag,pws,verbose=True)
  if(allE==None):
    sys.exit(3)

  # a dictionary, keyed by entry.id.text with value being a list: [ [commentlist] ]
  masterDict = {}

  # get all comments on each image in the entrylist
  for e in allE:
    print e.GetCommentsUri()
    masterDict[e.id.text] = [e,[]]
    c = pws.GetFeed(e.GetCommentsUri())
    print "found %d comments for image %s" % (len(c.entry),e.id.text)
    for ce in c.entry:
      for cauthor in ce.author:
        if cauthor.user.text.lower()==serviceuser or len(serviceuser)==0:
          masterDict[e.id.text][1].append(ce.content.text)

  for candidate in masterDict:
    for c in masterDict[candidate][1]:
      print candidate+"; comment="+c
      #print getPhotoPageHref(masterDict[candidate][0])+"; comment="+c

  #albumFeedURIs = getUniqueAlbumFeedURIs(allE)
  #albumFeeds = []
  #for a in albumFeedURIs:
  #  ff=pws.GetFeed(a)
  #  albumFeeds.append(ff)
    #NumCommentsWithTag = 0
    #for c in ff.entry:
    #  thisCommentAuthor = c.author[0].user.text.lower()
    #  if(thisCommentAuthor==serviceuser):
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


  #print "Checking for comments by user "+serviceuser



if __name__ == '__main__':
  main()
