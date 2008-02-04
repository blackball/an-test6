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
AUTH_USR = "GDATA_PW_AUTHUSER"

globalPWService = None

def pwInit():
  global globalPWService
  if globalPWService:
    return globalPWService
  try:
    globalPWService=gdata.photos.service.PhotosService()
    return globalPWService
  except:
    print "Error creating GData service. Are all the relevant python libraries installed?"
    sys.exit(1)


def pwAuth(pws=None,gdata_authtoken=None,gdata_user=None):
  from os import environ
  if pws==None:
    pws=pwInit()
  if gdata_authtoken==None and AUTH_ENV in environ:
    gdata_authtoken=environ[AUTH_ENV]
  if gdata_user==None and AUTH_USR in environ:
    gdata_user=environ[AUTH_USR]

  if gdata_authtoken==None:
    print "Must authenticate and export auth token to environment variable "+AUTH_ENV
    sys.exit(3)
  if gdata_user==None:
    print "Must authenticate and export username to environment variable "+AUTH_USR
    sys.exit(3)
  else:
    pws.auth_token=gdata_authtoken
    pws._GDataService__auth_token=gdata_authtoken
    pws.email=gdata_user


def pwParseBasicOpts(extraOpts,extraDefaults,HELPSTRING):
  HELPSTRING_BASE = ' [--user=photo_user] [--auth="auth token"]'

  baseopts = ["user=","authtoken=","auth="]
  baseopts=extraOpts+baseopts

  # parse command line options
  try:
    opts, args = getopt.getopt(sys.argv[1:], "", baseopts)
  except getopt.error, msg:
    print   HELPSTRING+HELPSTRING_BASE
    sys.exit(2)

  puser=None
  gdata_authtoken=None

  # Process options
  for o, a in opts:
    if o == "--auth" or o == "--authtoken":
      gdata_authtoken=a
    elif o == "--user":
      puser=a.lower()
    else:
      for i in range(len(extraOpts)):
        if o == '--'+extraOpts[i][:-1]:
          extraDefaults[i]=a

  return extraDefaults+[puser,gdata_authtoken]


def pwParsePhotoOpts(extraOpts,extraDefaults,HELPSTRING):
  HELPSTRING_BASE = ' [--user=photo_user] [--auth="auth token"]'
  THISHELP=" --albumid=albumid --photoid=photo_id"

  rez=pwParseBasicOpts(extraOpts+["photoid=","albumid="],extraDefaults+[None,None],HELPSTRING+THISHELP)
  if rez[len(extraOpts)]==None:
    print "PhotoID cannot be missing."
    print HELPSTRING+THISHELP+HELPSTRING_BASE
    sys.exit(2)
  if rez[len(extraOpts)+1]==None:
    print "AlbumID cannot be missing."
    print HELPSTRING+THISHELP+HELPSTRING_BASE
    sys.exit(2)
  return rez


def uploadPhoto(photofile,palbum,tag=None,caption=None,verbose=False,pws=None):
  if pws==None:
    pws=pwInit()
  if verbose:
    print "  Adding photo in file %s to album %s of user %s" % (photofile,palbum,pws.email)
  if verbose and caption:
    print "  ...setting caption to %s" % caption
  if verbose and tag:
    print "  ...tagging with tags %s" % tag
  try:
    albumURI='http://picasaweb.google.com/data/feed/api/user/'+pws.email+'/albumid/'+palbum
    pEntry=pws.InsertPhotoSimple(albumURI,photofile.split('/')[-1],caption,photofile,keywords=tag)
    if verbose:
      print "  New photoid=%s [%sx%s pixels, %s bytes]" % \
            (pEntry.gphoto_id.text,pEntry.width.text,pEntry.height.text,pEntry.size.text)
    return pEntry
  except:
    print "  Error inserting photo. Make sure file exists and auth token is not expired?"
    return None

def insertTag(tag,pphotoid,palbum,puser=None,verbose=False,pws=None):
  if pws==None:
    pws=pwInit()
  if puser==None:
    puser=pws.email
  photoURI='http://picasaweb.google.com/data/feed/api/user/'+puser+'/albumid/'+palbum+'/photoid/'+pphotoid
  # CHECK IF TAG IS ALREADY THERE?
  try:
    pws.InsertTag(photoURI,tag)
  except:
    print "  Error adding tag to "+photoURI



def insertComment(comment,pphotoid,palbum,puser=None,verbose=False,pws=None):
  if pws==None:
    pws=pwInit()
  if puser==None:
    puser=pws.email
  e=getPhotoEntry(palbum,pphotoid,puser=puser)
  if e:
    try:
      pws.InsertComment(e,comment)
    except:
      print "  Error inserting comment. Make sure auth token is not expired?"
  else:
    print "  Unable to get photoentry to add comment."



def setCaption(caption,pphotoid,palbum,pentry=None,puser=None,verbose=False,pws=None):
  if pws==None:
    pws=pwInit()
  if pentry==None:
    if puser==None:
      puser=pws.email
    pentry=getPhotoEntry(palbum,pphotoid,puser=puser,pws=pws)
  if pentry:
    pentry.summary.text=caption
    try:
      pws.UpdatePhotoMetadata(pentry)
    except:
      print "  Could not update metadata for photoid %s" % pentry.gphoto_id.text
  else:
    print "  Could not get photo entry for photoid %s, album %s, user %s" % (pphotoid,palbum,puser)


def makeEntryFilename(e):
  pwuseremail=e.id.text[e.id.text.find('/user/')+6:].split('/')[0]
  pwalbumid=e.id.text[e.id.text.find('/albumid/')+9:].split('/')[0]
  pwphotoid=e.id.text[e.id.text.find('/photoid/')+9:].split('/')[0]
  localfilename=pwuseremail.lower()+"_"+pwalbumid+"_"+pwphotoid+"@"+e.content.src.split('/')[-1]
  return localfilename

def makeAlbumnameFromMD5(md5sum):
  return "%03d" % int(int(md5sum,16)%498+1)
  
def downloadEntry(e,verbose=False,skipdownload=False):
  from urllib import urlretrieve,urlcleanup
  from os import popen
  from time import ctime
  localfilename=makeEntryFilename(e)
  if verbose:
    print ctime()
    print "  "+e.id.text
    print "  "+e.content.src
    print "  -->"+localfilename
  if skipdownload:
    return (None,None)
  else:
    urlcleanup()
    urlretrieve(e.content.src,filename=localfilename)
    md5pipe=popen('md5sum '+localfilename)
    md5out=md5pipe.read()
    md5sum=md5out.split()[0]
    md5pipe.close()
    print "  md5sum=%s" % md5sum
    return (localfilename,md5sum)


def getPhotoEntry(palbum,pphotoid,puser=None,pws=None):
  if pws==None:
    pws=pwInit()
  if puser==None:
    puser=pws.email
  albumfeed='http://picasaweb.google.com/data/feed/api/user/'+puser+'/albumid/'+palbum+'/?kind=photo'
  af=pws.GetFeed(albumfeed)
  for e in af.entry:
    ll=e.GetFeedLink().href
    if ll[ll.rfind('/photoid/')+9:]==pphotoid:
      return e
  return None


def getAllTagEntries(tag,puser=None,verbose=False,pws=None):
  if pws==None:
    pws=pwInit()
  if verbose:
    print "  Querying for images having tag="+tag+"..."
  allE=[]
  if puser==None:
    theserez=pws.GetFeed("http://picasaweb.google.com/data/feed/api/all?q="+tag)
  else:
    theserez=pws.GetFeed("http://picasaweb.google.com/data/feed/api/user/"+puser+"?q="+tag)
  numToGet = int(theserez.total_results.text)
  if verbose:
    print "  ...trying to get %d results" % numToGet
  if(len(theserez.entry)==0 or numToGet==0):
    if verbose:
      print "  No matching images found. Sorry."
    return None
  else:
    while(len(allE)<numToGet):
      #if verbose:
      #  print "  ...now have %d results, doing append" % len(allE)
      allE.extend(theserez.entry)
      #if verbose:
      #  print "  ...did append now have %d results, getting feed again" % len(allE)
      try:
        theserez=pws.GetFeed("http://picasaweb.google.com/data/feed/api/all?q="+tag, 
                             start_index=len(allE)+1) # check the +1
      except:
        print "  Warning: only got %d of %d results" % (len(allE),numToGet)
        break
      #if verbose:
      #  print "  ...got feed, looping back in while"
    if verbose:
      print "  Retrieved data for %d (of %d) matching images." % (len(allE),numToGet)
    return allE



def getAllAlbums(username,verbose=False,pws=None):
  if pws==None:
    pws=pwInit()
  if verbose:
    print "  Querying for albums from user="+username+"..."
  allA=[]
  try:
    thisfeed=pws.GetFeed("http://picasaweb.google.com/data/feed/api/user/"+username+"?kind=album")
    return thisfeed.entry
  except:
    if verbose:
      print "  Unable to find albums for user="+username+"..."
    return None



def insertAlbumNonDuplicate(username,albumtitle,alist=None,verbose=False,pws=None):
  if pws==None:
    pws=pwInit()
  if alist==None:
    alist = getAllAlbums(username,verbose=verbose,pws=pws)
  albumtitles=[e.title.text for e in alist]
  if albumtitle not in albumtitles:
    if verbose:
      print "  Existing albums for user="+username+": "+str(albumtitles)
      print "  Inserting album="+albumtitle
    try:
      return pws.InsertAlbum(albumtitle,"album summary")
    except:
      if verbose:
        print "  Error inserting album="+albumtitle+" for user="+username+"..."
      return None
  else:
    if verbose:
      print "  Album="+albumtitle+" already exists for user="+username+"."
    return alist[albumtitles.index(albumtitle)]
  

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


def getPhotoComments(e,pws=None):
  if pws==None:
    pws=pwInit()
  return int(pws.GetFeed(e.GetCommentsUri()).commentCount.text)


