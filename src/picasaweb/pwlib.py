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
import sys
import getopt

AUTH_ENV = "GDATA_PW_AUTHTOKEN"
HELPSTRING_BASE = ' --user=photo_user --album=album_id [--auth="auth token"]'

def pwInit():
  try:
    pws = gdata.photos.service.PhotosService()
  except:
    print "Error creating GData service. Are all the relevant python libraries installed?"
    sys.exit(1)
  return pws


def pwAuth(pws,gdata_authtoken):
  from os import environ
  if gdata_authtoken=='' and AUTH_ENV in environ:
    gdata_authtoken=environ[AUTH_ENV]

  if gdata_authtoken=='':
    print "Must authenticate and export auth token to environment variable "+AUTH_ENV
    sys.exit(3)
  else:
    pws.auth_token=gdata_authtoken
    pws._GDataService__auth_token=gdata_authtoken



def pwParseBasicOpts(extraOpts,extraDefaults,HELPSTRING):
  baseopts = ["authtoken=","auth=","user=","album="]
  baseopts=extraOpts+baseopts

  # parse command line options
  try:
    opts, args = getopt.getopt(sys.argv[1:], "", baseopts)
  except getopt.error, msg:
    print   HELPSTRING+HELPSTRING_BASE
    sys.exit(2)

  gdata_authtoken=''
  puser=''
  palbum=''

  # Process options
  for o, a in opts:
    if o == "--auth" or o == "--authtoken":
      gdata_authtoken=a
    elif o == "--user":
      puser=a.lower()
    elif o == "--album":
      palbum=a
    else:
      for i in range(len(extraOpts)):
        if o == '--'+extraOpts[i][:-1]:
          extraDefaults[i]=a
  if puser=='' or palbum=='':
    print HELPSTRING+HELPSTRING_BASE
    sys.exit(2)

  return extraDefaults+[puser,palbum,gdata_authtoken]


def pwParsePhotoOpts(extraOpts,extraDefaults,HELPSTRING):
  rez=pwParseBasicOpts(extraOpts+["photoid="],extraDefaults+[""],HELPSTRING+" --photoid=photo_id")
  if rez[len(extraOpts)]=="":
    print "PhotoID cannot be missing."
    print HELPSTRING+" --photoid=photo_id"+HELPSTRING_BASE
    sys.exit(2)
  return rez



def GetPhotoEntry(pws,puser,palbum,pphotoid):
  albumfeed='http://picasaweb.google.com/data/feed/api/user/'+puser+'/albumid/'+palbum+'/?kind=photo'
  af=pws.GetFeed(albumfeed)
  for e in af.entry:
    ll=e.GetFeedLink().href
    if ll[ll.rfind('/photoid/')+9:]==pphotoid:
      return e
  return None


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
