#!/usr/bin/python

__author__ = 'Sam Roweis'

from pwlib import *
from time import time
import sys


def main():
  """Continuously polls for new images that have a certain tag value using GData Picasaweb API"""

  HELPSTRING = 'pollforTag.py --tag=DesiredTag'
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
    if allE:
      for e in allE:
        if e.id.text not in masterDict:
          masterDict[e.id.text] = time()
          downloadEntry(e,verbose=True,skipdownload=True)

if __name__ == '__main__':
  main()
