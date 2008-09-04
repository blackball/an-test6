#! /usr/bin/env python

from watcher_common import *
from astrometry.net import settings

#from astrometry.net.ice import IceSolver
import IceSolver

class WatcherIce(Watcher):
    def solve_job(self, job):
        blindlog = job.get_filename('blind.log')

        def userlog(msg):
            f = open(blindlog, 'a')
            f.write(msg)
            f.close()

        axy = read_file(job.get_axy_filename())

        tardata = IceSolver.solve(job.jobid, axy, userlog)
        return tardata

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Usage: %s <input-file>' % sys.argv[0]
        sys.exit(-1)
    joblink = sys.argv[1]
    w = WatcherIce()
    sys.exit(w.main(joblink))

