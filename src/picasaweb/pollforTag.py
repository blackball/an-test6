#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *

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
    allE = getAllTagEntries(tag,pws,puser=user)
    if(allE==None):
      print "Error -- no images with tag=%s found" % tag
      sys.exit(3)
    # add all new ones to dictionary
    for e in allE:
      if e.id.text not in masterDict:
        masterDict[e.id.text] = 0
        print e.id.text
        print e.content.src


if __name__ == '__main__':
  main()
