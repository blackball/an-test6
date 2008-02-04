#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
from time import time
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
    # get new urls as local files
    for e in allE:
      if e.id.text not in masterDict:
        masterDict[e.id.text] = time()
        (localfilename,md5sum)=downloadEntry(e,verbose=True)
        albumname=makeAlbumnameFromMD5(md5sum)
        photouser=e.id.text[e.id.text.find('/user/')+6:].split('/')[0]
        captionText="%s from %s" % (e.content.src.split('/')[-1],photouser)
        aa=insertAlbumNonDuplicate(pws.email,albumname,verbose=True)
        palbum=aa.gphoto_id.text
        tags=["SkyPhotoLocatorUser:"+photouser]
        #uploadPhoto(localfilename,palbum,caption=captionText,tag=tags,verbose=True)
        uploadPhoto(localfilename,palbum,verbose=True)


if __name__ == '__main__':
  main()


