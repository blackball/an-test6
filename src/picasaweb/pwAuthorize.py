#!/usr/bin/python

__author__ = 'Sam Roweis'

from os import environ
import getpass

try:
  from xml.etree import ElementTree
except ImportError:
  from elementtree import ElementTree
import gdata.photos, gdata.photos.service
import gdata.service

import getopt
import sys

HELPSTRING="Usage: pwAuthorize [--user=uname] [--pass=pwd]"
 
def main():
  """Authorizes a user to PW GData Api Services"""

  # parse command line options
  try:
    opts, args = getopt.getopt(sys.argv[1:], "", ["user=","pass=","password="])
  except getopt.error, msg:
    print HELPSTRING
    sys.exit(2)

  try:
    pws = gdata.photos.service.PhotosService()
  except:
    print "Error creating GData service. Are all the relevant python libraries installed?"
    sys.exit(1)

  uname = None
  pwrd = None

  # Process options
  for o, a in opts:
    if o == "--user":
      uname = a.lower()
    if o == "--password" or o == "--pass":
      pwrd = a
  if uname==None:
    uname=str(raw_input("Picasaweb Username:")).lower()
  if pwrd ==None:
    pwrd=getpass.getpass("password for user "+uname+":")

  try:
    pws.ClientLogin(uname, pwrd)
  except:
    print "Error logging into service. Do username and password match?"
    sys.exit(1)


  shell='sh'
  if "SHELL" in environ and environ["SHELL"].find('csh')>=0:
    shell='csh'

  if shell=='csh':
    print 'setenv GDATA_PW_AUTHTOKEN "'+pws.auth_token+'"'
    print 'setenv GDATA_PW_AUTHUSER "'+pws.email+'"'
  else:
    print 'export GDATA_PW_AUTHTOKEN="'+pws.auth_token+'"'
    print 'export GDATA_PW_AUTHUSER="'+pws.email+'"'

if __name__ == '__main__':
  main()
