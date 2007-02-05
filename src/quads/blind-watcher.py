from pyinotify import WatchManager, Notifier, ThreadedNotifier, EventsCodes, ProcessEvent
import os

wm = WatchManager()

# IN_CLOSE_WRITE
# IN_MOVED_TO
# IN_UNMOUNT
mask = EventsCodes.IN_DELETE | EventsCodes.IN_CREATE  # watched events

class PTmp(ProcessEvent):
    def process_IN_CREATE(self, event):
        print "Create: %s" %  os.path.join(event.path, event.name)

    def process_IN_DELETE(self, event):
        print "Remove: %s" %  os.path.join(event.path, event.name)


notifier = Notifier(wm, PTmp())
#wdd = wm.add_watch('/tmp', mask, rec=True)
wdd = wm.add_watch('/tmp', mask)

while True:  # loop forever
    try:
        # process the queue of events as explained above
        notifier.process_events()
        if notifier.check_events():
            # read notified events and enqeue them
            notifier.read_events()
        # you can do some tasks here...
    except KeyboardInterrupt:
        break


#wm.rm_watch(wdd['/tmp'])
wm.rm_watch(wdd.values())
# destroy the inotify's instance on this interrupt (stop monitoring)
notifier.stop()

