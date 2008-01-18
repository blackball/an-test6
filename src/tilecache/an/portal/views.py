import datetime
import logging
import math
import os
import os.path
import random
import sha
import tempfile
import time

import django.contrib.auth as auth
from django.contrib.auth.decorators import login_required

from django import newforms as forms
from django.db import models
from django.http import HttpResponse, HttpResponseRedirect
from django.newforms import widgets, ValidationError, form_for_model
from django.template import Context, RequestContext, loader
from django.core.urlresolvers import reverse

import an.portal.mercator as merc

from an.portal.models import UserPreferences

from an.portal.job import Job, Submission, AstroField

from an.portal.convert import convert
from an.portal.log import log

from an.util.file import file_size

from an.vo.models import Image as voImage

from an import gmaps_config
from an import settings

import sip

# Adding a user:
# > python manage.py shell
# >>> from django.contrib.auth.models import User
# >>> email = 'user@domain.com'
# >>> passwd = 'password'
# >>> user = User.objects.create_user(email, email, passwd)
# >>> user.save()

def get_status_url(jobid):
    return reverse(jobstatus) + '?jobid=' + jobid

def get_job(jobid):
    jobs = Job.objects.all().filter(jobid=jobid)
    if len(jobs) != 1:
        #log('Found %i jobs, not 1' % len(jobs))
        return None
    job = jobs[0]
    return job

def get_url(job, fn):
    return reverse(getfile) + '?jobid=%s&f=%s' % (job.jobid, fn)

def getsessionjob(request):
    if not 'jobid' in request.session:
        log('no jobid in session')
        return None
    jobid = request.session['jobid']
    return get_job(jobid)

@login_required
def submission_status(request, submission):
    jobs = submission.jobs.all().order_by('starttime', 'jobid')
    ctxt = {
        'submission' : submission,
        'jobs' : jobs,
        'statusurl' : get_status_url(''),
        }
    t = loader.get_template('portal/submission_status.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def jobstatus(request):
    if not request.GET:
        return HttpResponse('no GET')
    if not 'jobid' in request.GET:
        return HttpResponse('no jobid')
    jobid = request.GET['jobid']
    log('jobstatus: jobid=', jobid)
    job = get_job(jobid)
    if not job:
        log('job not found.')
        submissions = Submission.objects.all().filter(jobid=jobid)
        log('found %i submissions.' % len(submissions))
        if len(submissions):
            submission = submissions[0]
            log('submission: ', submission)
            jobs = submission.jobs.all()
            log('submission has %i jobs.' % len(jobs))
            if len(jobs) == 1:
                job = jobs[0]
                jobid = job.jobid
                log('submission has one job:', jobid)
                return HttpResponseRedirect(get_status_url(jobid))
            else:
                return submission_status(request, submission)
        else:
            return HttpResponse('no job with jobid ' + jobid)
    log('job found.')

    jobowner = (job.submission.user == request.user)
    anonymous = job.allowanonymous()
    if not (jobowner or anonymous):
        return HttpResponse('The owner of this job (' + job.submission.user.username + ') has not granted public access.')

    field = job.field
    submission = job.submission
    log('jobstatus: Job is: ' + str(job))

    ctxt = {
        'jobid' : job.jobid,
        'jobstatus' : job.status,
        'jobsolved' : job.solved,
        'jobsubmittime' : submission.format_submittime(),
        'jobstarttime' : job.format_starttime(),
        'jobfinishtime' : job.format_finishtime(),
        'logurl' : get_url(job, 'blind.log'),
        'job' : job,
        'submission' : job.submission,
        'joburl' : (submission.datasrc == 'url') and submission.url or None,
        'jobfile' : (submission.datasrc == 'file') and submission.uploaded.userfilename or None,
        'jobscale' : job.friendly_scale(),
        'jobparity' : job.friendly_parity(),
        'sources' : get_url(job, 'sources'),
        'sources_big' : get_url(job, 'sources-big'),
        'jobowner' : jobowner,
        'allowanon' : anonymous,
        }

    if job.solved:
        wcsinfofn = convert(job, job.field, 'wcsinfo')
        f = open(wcsinfofn)
        wcsinfotxt = f.read()
        f.close()
        wcsinfo = {}
        for ln in wcsinfotxt.split('\n'):
            s = ln.split(' ')
            if len(s) == 2:
                wcsinfo[s[0]] = s[1]

        ctxt.update({'racenter' : '%.2f' % float(wcsinfo['ra_center']),
                     'deccenter': '%.2f' % float(wcsinfo['dec_center']),
                     'fieldw'   : '%.2f' % float(wcsinfo['fieldw']),
                     'fieldh'   : '%.2f' % float(wcsinfo['fieldh']),
                     'fieldunits': wcsinfo['fieldunits'],
                     'racenter_hms' : wcsinfo['ra_center_hms'],
                     'deccenter_dms' : wcsinfo['dec_center_dms'],
                     'orientation' : '%.3f' % float(wcsinfo['orientation']),
                     'pixscale' : '%.4g' % float(wcsinfo['pixscale']),
                     'parity' : (float(wcsinfo['det']) > 0 and 'Positive' or 'Negative'),
                     'wcsurl' : get_url(job, 'wcs.fits'),
                     })

        objsfn = convert(job, job.field, 'objsinfield')
        f = open(objsfn)
        objtxt = f.read()
        f.close()
        objs = objtxt.strip()
        if len(objs):
            objs = objs.split('\n')
        else:
            objs = []
        ctxt['objsinfield'] = objs

        # deg
        fldsz = math.sqrt(field.imagew * field.imageh) * float(wcsinfo['pixscale']) / 3600.0

        url = (gmaps_config.tileurl + '?layers=tycho,grid,userboundary' +
               '&arcsinh&wcsfn=%s' % job.get_relative_filename('wcs.fits'))
        smallstyle = '&w=300&h=300&lw=3'
        largestyle = '&w=1024&h=1024&lw=5'
        steps = [ {              'gain':0,    'dashbox':0.1,   'center':False },
                  {'limit':18,   'gain':-0.5, 'dashbox':0.01,  'center':True, 'dm':0.05 },
                  {'limit':1.8,  'gain':0.25,                   'center':True, 'dm':0.005 },
                  ]
        zlist = []
        for last_s in range(len(steps)):
            s = steps[last_s]
            if 'limit' in s and fldsz > s['limit']:
                log('break')
                break
        else:
            last_s = len(steps)

        for ind in range(last_s):
            s = steps[ind]
            if s['center']:
                xmerc = float(wcsinfo['ra_center_merc'])
                ymerc = float(wcsinfo['dec_center_merc'])
                ralo = merc.merc2ra(xmerc + s['dm'])
                rahi = merc.merc2ra(xmerc - s['dm'])
                declo = merc.merc2dec(ymerc - s['dm'])
                dechi = merc.merc2dec(ymerc + s['dm'])
                bb = [ralo, declo, rahi, dechi]
            else:
                bb = [0, -85, 360, 85]
            urlargs = ('&gain=%g' % s['gain']) + ('&bb=' + ','.join(map(str, bb)))
            if (ind < last_s) and ('dashbox' in s):
                urlargs += '&dashbox=%g' % s['dashbox']

            zlist.append([url + smallstyle + urlargs,
                          url + largestyle + urlargs])

        # HACK
        fn = convert(job, job.field, 'fullsizepng')
        url = (gmaps_config.gmaps_url +
               ('?zoom=%i&ra=%.3f&dec=%.3f&userimage=%s' %
                (int(wcsinfo['merczoom']), float(wcsinfo['ra_center']),
                 float(wcsinfo['dec_center']), job.get_relative_job_dir())))

        ctxt.update({
            'gmapslink' : url,
            'zoomimgs'  : zlist,
            'annotation': get_url(job, 'annotation'),
            'annotation_big' : get_url(job, 'annotation-big'),
            })

    else:
        logfn = job.get_filename('blind.log')
        if os.path.exists(logfn):
            f = open(logfn)
            logfiletxt = f.read()
            f.close()
            lines = logfiletxt.split('\n')
            lines = '\n'.join(lines[-16:])
            log('job not solved')

            ctxt.update({
                'logfile_tail' : lines,
                })

    t = loader.get_template('portal/status.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

def getfield(request):
    if not 'fieldid' in request.GET:
        return HttpResponse('no fieldid')
    fieldid = request.GET['fieldid']
    fields = AstroField.objects.all().filter(id=fieldid)
    if not len(fields):
        return HttpResponse('no such field')
    field = fields[0]

    owner = (field.user == request.user)
    anonymous = field.allowredist
    if not (owner or anonymous):
        return HttpResponse('The owner of this job (' + field.user.username + ') has not granted public access.')

    res = HttpResponse()
    ct = field.content_type()
    res['Content-Type'] = ct or 'application/octet-stream'
    fn = field.filename()
    res['Content-Length'] = file_size(fn)
    #res['Content-Disposition'] = 'attachment; filename="' + f + '"'
    res['Content-Disposition'] = 'inline'
    f = open(fn)
    res.write(f.read())
    f.close()
    return res

def getfile(request):
    if not request.GET:
        return HttpResponse('no GET')
    if 'fieldid' in request.GET:
        return getfield(request)
    if not 'jobid' in request.GET:
        return HttpResponse('no jobid')
    jobid = request.GET['jobid']
    job = get_job(jobid)
    if not job:
        return HttpResponse('no such job')
    if not 'f' in request.GET:
        return HttpResponse('no f=')

    jobowner = (job.submission.user == request.user)
    anonymous = job.allowanonymous()
    if not (jobowner or anonymous):
        return HttpResponse('The owner of this job (' + job.submission.user.username + ') has not granted public access.')

    f = request.GET['f']

    res = HttpResponse()

    pngimages = [ 'annotation', 'annotation-big',
                  'sources', 'sources-big',
                  'redgreen', 'redgreen-big' ]
    if f in pngimages:
        fn = convert(job, job.field, f)
        res['Content-Type'] = 'image/png'
        res['Content-Length'] = file_size(fn)
        f = open(fn)
        res.write(f.read())
        f.close()
        return res

    binaryfiles = [ 'wcs.fits', 'match.fits' ]
    if f in binaryfiles:
        fn = job.get_filename(f)
        res['Content-Type'] = 'application/octet-stream'
        res['Content-Disposition'] = 'attachment; filename="' + f + '"'
        res['Content-Length'] = file_size(fn)
        f = open(fn)
        res.write(f.read())
        f.close()
        return res

    textfiles = [ 'blind.log' ]
    if f in textfiles:
        fn = job.get_filename(f)
        res['Content-Type'] = 'text/plain'
        res['Content-Disposition'] = 'inline'
        res['Content-Length'] = file_size(fn)
        f = open(fn)
        res.write(f.read())
        f.close()
        return res

    return HttpResponse('bad f')

def printvals(request):
    if request.POST:
        log('POST values:')
        for k,v in request.POST.items():
            log('  %s = %s' % (str(k), str(v)))
    if request.GET:
        log('GET values:')
        for k,v in request.GET.items():
            log('  %s = %s' % (str(k), str(v)))
    if request.FILES:
        log('FILES values:')
        for k,v in request.FILES.items():
            log('  %s = %s' % (str(k), str(v)))

@login_required
def userprefs(request):
    prefs = UserPreferences.for_user(request.user)
    PrefsForm = form_for_model(UserPreferences)
    if request.POST:
        form = PrefsForm(request.POST)
    else:
        form = PrefsForm({
            'autoredistributable': prefs.autoredistributable,
            'anonjobstatus': prefs.anonjobstatus,
            })

    if request.POST and form.is_valid():
        prefs.autoredistributable = form.cleaned_data['autoredistributable']
        prefs.anonjobstatus = form.cleaned_data['anonjobstatus']
        prefs.save()
        msg = 'Preferences Saved'
    else:
        msg = None
        
    ctxt = {
        'msg' : msg,
        'form' : form,
        }
    t = loader.get_template('portal/userprefs.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))

@login_required
def summary(request):
    prefs = UserPreferences.for_user(request.user)
    submissions = Submission.objects.all().filter(user=request.user)
    jobs = []
    for sub in submissions:
        jobs += sub.jobs.all()

    for job in jobs:
        log('Job ', job, 'allow anon?', job.allowanonymous(prefs))
        log('Job.allowanon:', job.allowanon, ' forbidanon:', job.forbidanon)
        log('prefs:', prefs)
        log('Redist:', job.field.redistributable())
        log('Field:', job.field)

    voimgs = voImage.objects.all().filter(user=request.user)

    ctxt = {
        'jobs' : jobs,
        'voimgs' : voimgs,
        'prefs' : prefs,
        'statusurl' : get_status_url(''),
        'getfileurl' : reverse(getfile) + '?jobid=',
        'getfield' : reverse(getfile) + '?fieldid=',
        }
    t = loader.get_template('portal/summary.html')
    c = RequestContext(request, ctxt)
    return HttpResponse(t.render(c))
    
@login_required
def changeperms(request):
    if not request.POST:
        return HttpResponse('no POST')
    prefs = UserPreferences.for_user(request.user)

    if 'fieldid' in request.POST:
        fid = int(request.POST['fieldid'])
        fields = AstroField.objects.all().filter(id = fid)
        if not fields or len(fields) != 1:
            return HttpResponse('no field')
        field = fields[0]
        if field.user != request.user:
            return HttpResponse('not your field!')
        if 'redist' in request.POST:
            redist = int(request.POST['redist'])
            if redist:
                field.forbidredist = False
                if not prefs.autoredistributable:
                    # need to explicitly allow this one.
                    field.allowredist = True
            else:
                field.allowredist = False
                if prefs.autoredistributable:
                    # need to explicitly forbid.
                    field.forbidredist = True
            field.save()
            if 'HTTP_REFERER' in request.META:
                return HttpResponseRedirect(request.META['HTTP_REFERER'])
            return HttpResponseRedirect(reverse(summary))
            
        return HttpResponse('no action.')

    if 'jobid' in request.POST:
        jobid = request.POST['jobid']
        jobs = Job.objects.all().filter(jobid = jobid)
        if not jobs or len(jobs) != 1:
            return HttpResponse('no job')
        job = jobs[0]
        if job.submission.user != request.user:
            return HttpResponse('not your job!')
        if 'allowanon' in request.POST:
            allow = int(request.POST['allowanon'])
            if allow:
                job.forbidanon = False
                if not prefs.anonjobstatus:
                    # need to explicitly allow this one.
                    job.allowanon = True
            else:
                job.allowanon = False
                if prefs.anonjobstatus:
                    # need to explicitly forbid.
                    job.forbidanon = True
            job.save()
            if 'HTTP_REFERER' in request.META:
                return HttpResponseRedirect(request.META['HTTP_REFERER'])
            return HttpResponseRedirect(reverse(summary))

        return HttpResponse('no action.')

@login_required
def publishtovo(request):
    if not 'jobid' in request.POST:
        return HttpResponse('no jobid')
    job = get_job(request.POST['jobid'])
    if not job:
        return HttpResponse('no job')
    if job.submission.user != request.user:
        return HttpResponse('not your job')
    if not job.solved:
        return HttpResponse('job is not solved')
    wcs = job.tanwcs
    if not wcs:
        return HttpResponse('no wcs')


    # BIG HACK! - look through LD_LIBRARY_PATH if this is still needed...
    if not sip.libraryloaded():
        sip.loadlibrary('/home/gmaps/test/an-common/_sip.so')
    

    img = voImage(user = request.user,
                  field = job.field,
                  image_title = 'Field_%i' % (job.field.id),
                  instrument = '',
                  jdate = 0,
                  wcs = wcs,
                  )
    tanwcs = wcs.to_tanwcs()
    (ra, dec) = tanwcs.pixelxy2radec(wcs.imagew/2, wcs.imageh/2)
    (img.ra_center, img.dec_center) = (ra, dec)
    log('tanwcs: ' + str(tanwcs))
    log('(ra, dec) center: (%f, %f)' % (ra, dec))
    (ramin, ramax, decmin, decmax) = tanwcs.radec_bounds(10)
    (img.ra_min, img.ra_max, img.dec_min, img.dec_max) = (ramin, ramax, decmin, decmax)
    log('Saving vo.Image: ', img)
    img.save()

    return HttpResponse('Done.')
