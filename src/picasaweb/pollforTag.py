#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
from urllib import urlretrieve,urlcleanup
from time import time,ctime

import getopt
import sys


def main():
  """Continuously polls for new images that have a certain tag value using GData Picasaweb API"""

  HELPSTRING = 'pollforTag.py --tag=DesiredTag [--user=uname (not implemented yet)]'
  # parse command line options
  try:
    opts, args = getopt.getopt(sys.argv[1:], "", ["tag=","user="])
  except getopt.error, msg:
    print HELPSTRING
    sys.exit(2)

  tag = None
  uname = None

  # Process options
  for o, a in opts:
    if o == "--tag":
      tag = a.replace(' ','%20')
    if o == "--user":
      uname = a.lower()
  if tag==None:
    print HELPSTRING
    sys.exit(2)


  pws=pwInit()

  # a dictionary, keyed by entry.id.text with value being time seen
  masterDict = {}

  while True:
    allE = getAllTagEntries(tag,puser=uname)
    # get new urls as local files
    for e in allE:
      if e.id.text not in masterDict:
        pwuseremail=e.id.text[e.id.text.find('/user/')+6:].split('/')[0]
        pwalbumid=e.id.text[e.id.text.find('/albumid/')+9:].split('/')[0]
        pwphotoid=e.id.text[e.id.text.find('/photoid/')+9:].split('/')[0]
        masterDict[e.id.text] = time()
        print ctime()
        print "  "+e.id.text
        print "  "+e.content.src
        print "  -->"+pwuseremail+"_"+pwalbumid+"_"+pwphotoid


if __name__ == '__main__':
  main()
