# first need to apt-get install python-elementtree
# get gdata python tarball and untar AS ROOT
# (if you do it as a user, make sure root will have read, write and x(on dirs) perms)
# as root: ./setup.py install (or sudo ...)

try:
  from xml.etree import ElementTree
except ImportError:
  from elementtree import ElementTree
import gdata.photos, gdata.photos.service
import gdata.service

#import atom.service
#import atom


def getAllTagEntries(tag,pws):
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

def getPhotoComments(e,pws):
  return int(pws.GetFeed(e.GetCommentsUri()).commentCount.text)
