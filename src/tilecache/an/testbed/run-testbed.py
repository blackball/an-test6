import os
import os.path
import sys

os.environ['DJANGO_SETTINGS_MODULE'] = 'an.settings'

from django.contrib.auth.models import User

from an.util.run_command import run_command
from an.testbed.models import TestbedJob
from an.portal.wcs import TanWCS
from an.portal.job import AstroField, JobSet, Job


if __name__ == '__main__':

    tb = TestbedJob.objects.all()

    # HACK
    us = User.objects.all().filter(username='testbed@astrometry.net')
    if len(us) != 1:
        print 'Failed to find user.'
        sys.exit(-1)
    user = us[0]

    jobset = JobSet(
        jobid = Job.generate_jobid(),
        user = user,
        filetype = 'image',
        )
    #jobset.create_job_dir()
    jobset.save()

    print 'Created jobset', jobset.jobid

    for tbj in tb:
        job = Job(
            jobid = Job.generate_jobid(),
            field = tbj.field,
            jobset = jobset,
            )
        job.create_job_dir()
        job.save()
        Job.submit_job_or_jobset(job)
        print 'Created job', job.jobid

    print 'Created jobset', jobset.jobid
