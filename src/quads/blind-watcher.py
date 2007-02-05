#! /usr/bin/env python

from pyinotify import WatchManager, Notifier, ThreadedNotifier, EventsCodes, ProcessEvent
import os, signal
from Queue import Queue

def sigint(signum, frame):
    print 'SIGINT received.'
    eq.quit();

class Enqueuer(ProcessEvent):
    def __init__(self):
        self.q = Queue(0)

    def quit(self):
        self.q.put('QUIT');

    def get(self):
        try:
            return self.q.get(True)
        except KeyboardInterrupt:
            return 'QUIT'
        
    def handle(self, path):
        self.q.put(path)
        print "Queue has %i entries." % self.q.qsize()

    def process_IN_CLOSE_WRITE(self, event):
        print "Closed write: %s" %  os.path.join(event.path, event.name)
        self.handle(os.path.join(event.path, event.name));

    def process_IN_MOVED_TO(self, event):
        print "Moved in: %s" %  os.path.join(event.path, event.name)
        self.handle(os.path.join(event.path, event.name));



wm = WatchManager()
eq = Enqueuer()
signal.signal(signal.SIGINT, sigint)
notifier = ThreadedNotifier(wm, eq)
notifier.start()
mask = EventsCodes.IN_CLOSE_WRITE | EventsCodes.IN_MOVED_TO
wdd = wm.add_watch('/tmp', mask)

while True:
    try:
        filename = eq.get()
        print "Dequeued: %s" % filename;
        if (filename == 'QUIT'):
            break
    except KeyboardInterrupt:
        break

# un-watch
wm.rm_watch(wdd.values())
# destroy the inotify's instance on this interrupt (stop monitoring)
notifier.stop()

