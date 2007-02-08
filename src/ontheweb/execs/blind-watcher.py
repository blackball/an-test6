#! /usr/bin/env python

from pyinotify import WatchManager, Notifier, ThreadedNotifier, EventsCodes, ProcessEvent
import os, signal, time
from Queue import Queue
from subprocess import Popen

os.umask(0002); # in octal

BLIND = 'blind';
#IN_SUFFIX  = '.in';
IN_FILENAME  = 'input';
#OUT_SUFFIX = '.out';

queuefile = "queue";

def sigint(signum, frame):
    print 'SIGINT received.'
    eq.quit()

class Enqueuer(ProcessEvent):
    def __init__(self, qfile, emask):
        self.q = Queue(0)
        self.qlist = []
        self.qfile = qfile
        self.emask = emask

    def quit(self):
        self.q.put('QUIT')

    def get(self):
        try:
            item = self.q.get(True)
            self.qlist.remove(item)
            self.write_queue()
            return item
        except KeyboardInterrupt:
            return 'QUIT'

    def write_queue(self):
        f = open(self.qfile, "w")
        for item in self.qlist:
            f.write(item)
        f.close();

    def handle(self, path):
        #if (not(path.endswith(IN_SUFFIX))):
        print "Path changed: %s\n" % path
        if (not(os.path.basename(path) == IN_FILENAME)):
            return
        self.qlist.append(path)
        self.write_queue()
        print "Queue has %i entries." % self.q.qsize()
        self.q.put(path)

    def process_IN_CREATE(self, event):
        print "Created: %s\n" % event.name
        if (not(event.is_dir)):
            print "Not a directory.\n"
            return
        #time.sleep(5)
        wm.add_watch(event.name, self.emask)
        now = time.time()
        # check the directory for files created before "now".
        files = os.listdir(event.name)
        print "Now: %f" % now
        print "Listing of directory:"
        for f in files:
            st = os.stat(f)
            print "  %s: %f\n" % (f, st.st_mtime)

    def process_IN_CLOSE_WRITE(self, event):
        print "Closed write: %s" %  os.path.join(event.path, event.name)
        #self.handle(os.path.join(event.path, event.name));

    #def process_IN_MOVED_TO(self, event):
    #    print "Moved in: %s" %  os.path.join(event.path, event.name)
    #    self.handle(os.path.join(event.path, event.name));



mask = EventsCodes.IN_CLOSE_WRITE | EventsCodes.IN_MOVED_TO | EventsCodes.IN_CREATE | EventsCodes.IN_MODIFY
wm = WatchManager()
eq = Enqueuer(queuefile, mask)
signal.signal(signal.SIGINT, sigint)
notifier = ThreadedNotifier(wm, eq)
notifier.start()
cwd = os.getcwd()
print "Watching: %s\n" % cwd
#wdd = wm.add_watch(cwd, mask, rec=True, auto_add=True)
wdd = wm.add_watch(cwd, mask)
#print "Path: %s\n" % wm.get_path(wdd);

while True:
    try:
        filename = eq.get()
        print "Dequeued: %s" % filename;
        if (filename == 'QUIT'):
            break

        # run blind with the new input file.

        #outfile = filename[0:len(filename) - len(IN_SUFFIX)] + '.out'
        #print 'Output file: %s' % outfile
        #rtnval = os.system(BLIND + ' <' + filename + ' >' + outfile + ' 2>&1')
        rtnval = os.system(BLIND + ' <' + filename)
        if (rtnval):
            print 'Blind failed.'
        else:
            print 'Blind succeeded.'
    except KeyboardInterrupt:
        break

# un-watch
wm.rm_watch(wdd.values())
# destroy the inotify's instance on this interrupt (stop monitoring)
notifier.stop()

