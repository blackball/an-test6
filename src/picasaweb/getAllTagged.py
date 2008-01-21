#!/usr/bin/python

__author__ = 'Sam Roweis'


try:
  from xml.etree import ElementTree
except ImportError:
  from elementtree import ElementTree
import gdata.photos.service
#import gdata.service
#import atom.service
#import atom
import getopt
import sys
#import string
#import time

HELPSTRING = 'getAllTagged.py --tag DesiredTag --commentUser userid'

pws = gdata.photos.service.PhotosService()
#pws.ClientLogin(username, password)




def getAllTagEntries(tag):
  print "Querying for images having tag="+tag+"..."
  allE=[]
  theserez=pws.GetFeed("http://picasaweb.google.com/data/feed/api/all?q="+tag)
  numToGet = int(theserez.total_results.text)
  #print "...trying to get %d results" % numToGet
  if(len(theserez.entry)==0 or numToGet==0):
    print "No matching images found. Sorry."
    return None
  else:
    while(len(allE)<numToGet):
      #print "...now have %d results, doing append" % len(allE)
      allE.extend(theserez.entry)
      #print "...did append now have %d results, getting feed again" % len(allE)
      try:
        theserez=pws.GetFeed("http://picasaweb.google.com/data/feed/api/all?q="+tag, 
                             start_index=len(allE)+1) # check the +1
      except:
        #print "Warning: only got %d of %d results" % (len(allE),numToGet)
        break
      #print "...got feed, looping back in while"
    print "Retrieved data for %d (of %d) matching images." % (len(allE),numToGet)
    return allE

def getUniqueAlbumFeedURIs(entrylist):
  albumFeeds = {}
  for e in entrylist:
    href=e.GetFeedLink().href
    albumFeed = href[0:href.rfind('/photoid/')]+'?kind=comment'
    if albumFeed in albumFeeds:
      albumFeeds[albumFeed]+=1
    else:
      albumFeeds[albumFeed]=1
  return albumFeeds

def getAlbumTitle(e):
  for el in e.extension_elements:
    if el.tag=='albumtitle':
      return el.text
  return None

def getPhotoPageHref(e):
  for el in e.link:
    if el.type=='text/html':
      return el.href
  return None

def getPhotoComments(e):
  return int(pws.GetFeed(e.GetCommentsUri()).commentCount.text)
 
def main():
  """Gets all images that have a certain tag value using GData Picasaweb API"""

  # parse command line options
  try:
    opts, args = getopt.getopt(sys.argv[1:], "", ["tag=","commentUser="])
  except getopt.error, msg:
    print HELPSTRING
    sys.exit(2)

  tag = ''
  commentUser = 'astrometrydotnet'

  # Process options
  for o, a in opts:
    if o == "--tag":
      tag = a.replace(' ','%20')
    elif o == "--commentUser":
      commentUser = a.lower()
    else:
      print HELPSTRING
      sys.exit(2)

  allE = getAllTagEntries(tag)
  if(allE==None):
    sys.exit(3)
  allIDs = {}
  for e in allE:
    allIDs[e.id.text] = []

  print "Checking for comments by user "+commentUser
    
  albumFeedURIs = getUniqueAlbumFeedURIs(allE)
  albumFeeds = []
  for a in albumFeedURIs:
    ff=pws.GetFeed(a)
    albumFeeds.append(ff)
    NumCommentsWithTag = 0
    for c in ff.entry:
      thisCommentAuthor = c.author[0].user.text.lower()
      if(thisCommentAuthor==commentUser):
        thisCommentID = c.id.text
        thisID = thisCommentID[0:thisCommentID.rfind('/commentid/')]
        if thisID in allIDs:
          allIDs[thisID].append(c)
          NumCommentsWithTag += 1
    if(NumCommentsWithTag>0):
      print "%s <%s> has %d/%d tagged and %d/%d commented" % \
            (ff.user.text,ff.title.text,albumFeedURIs[a],int(ff.numphotos.text),\
             NumCommentsWithTag,len(ff.entry))

    
  #for e in allE:
  #  numComments = getPhotoComments(e)
  #  if(numComments>0):
  #    print "C: %d URL:%s" % (numComments,getPhotoPageHref(e))


if __name__ == '__main__':
  main()
