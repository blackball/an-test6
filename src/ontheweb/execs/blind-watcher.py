#! /usr/bin/env python

from pyinotify import WatchManager, Notifier, ThreadedNotifier, EventsCodes, ProcessEvent
import os, signal
from Queue import Queue
from subprocess import Popen

BLIND = 'blind';
#IN_SUFFIX  = '.in';
IN_FILENAME  = 'input';
#OUT_SUFFIX = '.out';

queuefile = "queue";

def sigint(signum, frame):
    print 'SIGINT received.'
    eq.quit()

class Enqueuer(ProcessEvent):
    def __init__(self, filename):
        self.q = Queue(0)
        self.qlist = []
        self.filename = filename

    def quit(self):
        self.q.put('QUIT')

    def get(self):
        try:
            item = self.q.get(True)
            self.qlist.remove(item)
            return item
        except KeyboardInterrupt:
            return 'QUIT'

    def write_queue(self):
        f = open(self.filename, "w")
        for item in self.qlist:
            f.write(item)
        f.close();

    def handle(self, path):
        #if (not(path.endswith(IN_SUFFIX))):
        if (not(os.basename(path) == IN_FILENAME)):
            print "Path changed: %s\n" % path
            return
        self.qlist.put(path)
        self.q.put(path)
        print "Queue has %i entries." % self.q.qsize()
        write_queue(self)

    def process_IN_CLOSE_WRITE(self, event):
        print "Closed write: %s" %  os.path.join(event.path, event.name)
        self.handle(os.path.join(event.path, event.name));

    def process_IN_MOVED_TO(self, event):
        print "Moved in: %s" %  os.path.join(event.path, event.name)
        self.handle(os.path.join(event.path, event.name));



wm = WatchManager()
eq = Enqueuer(queuefile)
signal.signal(signal.SIGINT, sigint)
notifier = ThreadedNotifier(wm, eq)
notifier.start()
mask = EventsCodes.IN_CLOSE_WRITE | EventsCodes.IN_MOVED_TO
cwd = os.getcwd()
print "Watching: %s\n" % cwd
wdd = wm.add_watch(cwd, mask, rec=True, auto_add=True)
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

