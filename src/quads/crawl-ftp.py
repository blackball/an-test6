import os
import re

from ftplib import FTP


class crawler(object):
    dirstack = [ '' ]
    currentdir = None
    #filelist = []
    #nomatches = []

    ffiles = None
    fdirs = None
    fnomatches = None

    def __init__(self):
        self.ffiles = open('files', 'wb')
        self.fdirs = open('dirs', 'wb')
        self.fnomatches = open('nomatch', 'wb')

    ire = re.compile(r'^(?P<dir>.)' + # 'd' or '-'
                     r'.{9}' + # mode
                     r'\s*\w*' + # space, link count
                     r'\s*\w*' + # space, owner
                     r'\s*\w*' + # space, group
                     r'\s*(?P<size>\d*)' + # filesize
                     r'\s*(?P<month>\w*)' + # space, month
                     r'\s*(?P<day>\w*)' + # space, day
                     r'\s*(?P<time>[\w:]*)' + # space, time
                     r'\s*(?P<name>[\w.~_-]*)' + # space, name
                     r'$')

    def close(self):
        self.ffiles.close()
        self.fdirs.close()
        self.fnomatches.close()

    def set_dirstack(self, stack):
        self.dirstack = stack

    def write_stack(self):
        f = open('dirstack.tmp', 'wb')
        for d in self.dirstack:
            f.write(d + '\n')
        f.close()
        os.rename('dirstack.tmp', 'dirstack')

    def add_item(self, s):
        print 'item', s
        m = self.ire.match(s)
        if not m:
            print 'no match'
            #nomatches.append(currentdir + '/' + s)
            self.fnomatches.write(self.currentdir + ' ' + s + '\n')
            self.fnomatches.flush()
            return
        #print 'dir:', m.group('dir')
        #print 'size:', m.group('size')
        #print 'month:', m.group('month')
        #print 'day:', m.group('day')
        #print 'time:', m.group('time')
        #print 'name:', m.group('name')

        d = m.group('dir')
        name = m.group('name')
        path = self.currentdir + '/' + name
        
        if d == 'd':
            self.dirstack.append(path)
            self.fdirs.write(path + '\n')
            self.fdirs.flush()
        else:
            #self.filelist.append(path)
            self.ffiles.write(path + '\n')
            self.ffiles.flush()



if __name__ == '__main__':
    ftp = FTP('galex.stsci.edu')
    ftp.login()

    crawl = crawler()

    crawl.currentdir = ''
    while len(crawl.dirstack):
        d = crawl.dirstack.pop()
        crawl.currentdir = d
        print 'listing "%s"' % d
        ftp.dir(d, crawl.add_item)
        crawl.write_stack()

    #print '\n\n\nFiles:\n\n\n'
    #for f in filelist:
    #    print f
    #print '\n\n\nUnmatched:\n\n\n'
    #for f in nomatches:
    #    print f
