#!/usr/bin/env python
"""
Solve a field.

Author: Dustin Lang 2007
"""
import sys
import os
import pyfits

hps = range(12)

indexes = (
    { 'name':'90degree' , 'quadl':1400, 'quadu':2000, 'paths':('100/index-119',) },
    { 'name':'60degree' , 'quadl':1000, 'quadu':1400, 'paths':('100/index-118',) },
    { 'name':'45degree' , 'quadl': 680, 'quadu':1000, 'paths':('100/index-117',) },
    { 'name':'30degree' , 'quadl': 480, 'quadu': 680, 'paths':('100/index-116',) },
    { 'name':'25degree' , 'quadl': 340, 'quadu': 480, 'paths':('100/index-115',) },
    { 'name':'15degree' , 'quadl': 240, 'quadu': 340, 'paths':('100/index-114',) },
    { 'name':'10degree' , 'quadl': 170, 'quadu': 240, 'paths':('100/index-113',) },
    { 'name':'8degree'  , 'quadl': 120, 'quadu': 170, 'paths':('100/index-112',) },
    { 'name':'5degree'  , 'quadl':  85, 'quadu': 120, 'paths':('100/index-111',) },
    { 'name':'4degree'  , 'quadl':  60, 'quadu':  85, 'paths':('100/index-110',) },
    { 'name':'2.5degree', 'quadl':  42, 'quadu':  60, 'paths':('100/index-109',) },
    { 'name':'2degree'  , 'quadl':  30, 'quadu':  42, 'paths':('100/index-108',) },
    { 'name':'1.5degree', 'quadl':  22, 'quadu':  30, 'paths':('100/index-107',) },
    { 'name':'1degree'  , 'quadl':  16, 'quadu':  22, 'paths':('100/index-106',) },
    { 'name':'40arcmin' , 'quadl':  11, 'quadu':  16, 'paths':('100/index-105',) },
    { 'name':'30arcmin' , 'quadl':   8, 'quadu':  11,
                    'paths':('100/index-104-%02i' % x for x in hps) },
    { 'name':'20arcmin' , 'quadl': 5.6, 'quadu':   8,
                    'paths':('100/index-103-%02i' % x for x in hps) },
    { 'name':'15arcmin' , 'quadl':   4, 'quadu': 5.6,
                    'paths':('100/index-102-%02i' % x for x in hps) },
    { 'name':'10arcmin' , 'quadl': 2.8, 'quadu':   4,
                    'paths':('100/index-101-%02i' % x for x in hps) },
    { 'name':'8arcmin'  , 'quadl':   2, 'quadu': 2.8,
                    'paths':('100/index-100-%02i' % x for x in hps) }
    )

depths = ( 0, 20, 30, 40, 50, 60, 70, 80, 90, 100, 120, 140, 160, 180, 200 );

totaltime = 0;
totalcpu = 600;
codetol = 0.01
distractors = 0.25
solved = "solved"

def log(*x):
    print >> sys.stderr, 'solve-field:', ' '.join(str(y) for y in x)

def die(*x):
    print >> sys.stderr, 'solve-field:', ' '.join(str(y) for y in x)
    sys.exit(-1)

def do_command(cmd):
    if os.system(cmd) != 0:
        print >>sys.stderr, 'Command failed: %s' % cmd
        sys.exit(-1)

def get_val_nothrow(fitshdr, key):
    try:
        val = fitshdr[key]
        return val
    except KeyError:
        return None

def solve_field(xyfile):
    # Read input file.
    try:
        fitsin = pyfits.open(xyfile)
    except IOError:
        log('Failed to read FITS input file "%s".  Exception is:' % xylist)
        raise
    return solve_fits(xyfile, fitsin)

# FIXME UNFINISHED
def extract_solver_parameters(xyfile, xyfits):
    # get from input file:
    # -image W, H
    # -scale ranges
    # -parity
    # -tweak
    # -tweak order
    # -poserr
    # -codetol ?
    # -match filename (out)
    # -rdls  filename (out)
    # -wcs   filename (out)

    # Print out info about input file.
    #fitsin.info()

    # First FITS extension...
    hdr = xyfits[1].header
    hdrcards = hdr.ascardlist()
    # FIXME log hdrcards
    
    width = hdr['IMAGEW']
    height = hdr['IMAGEH']

    parity = dict(POS=0, NEG=1, BOTH=2)[hdr.get('ANPARITY', 'BOTH')]
    poserr = float(hdr.get('ANPOSERR', 1.0))
    if poserr <= 0:
        poserr = 1.0
    matchfile  = hdr.get('ANMATCH')
    rdlsfile   = hdr.get('ANRDLS')
    wcsfile    = hdr.get('ANWCS')
    canfile    = hdr.get('ANCANCEL')
    tweak      = bool(hdr.get('ANTWEAK'))
    tweakorder = int(hdr.get('ANTWEAKO', 2))

    scales = []
    i = 0
    while True:
        keyl = 'ANAPPL%d' % i
        keyu = 'ANAPPU%d' % i
        #log keyl, keyu

        ful = hdr.get(keyl)
        fuu = hdr.get(keyu)
        if not ful or not fuu:
            break

        ful = float(ful)
        fuu = float(fuu)
        if ful == 0 or fuu == 0:
            break

        #log ful, fuu

        # Estimate size of quads we could find:
        fmax = 1.0 * max(width, height) * fuu / 60.0;
        fmin = 0.1 * min(width, height) * ful / 60.0;

        # Collect indices with overlapping scales:
        inds = []
        for v in indexes:
            ql = v['quadl']
            qu = v['quadu']
            if fmin <= qu and ql >= fmax:
                inds += v['paths']

        if len(inds) == 0:
            # bigger than the biggest?
            v = indexes[0]
            if fmin > v['quadu']:
                inds += v['paths']
            # smaller than the smallest?
            v = indexes[-1]
            if fmax < v['quadl']:
                inds += v['paths']

        scales.append((ful, fuu, inds))
        i += 1

# FIXME UNFINISHED
def make_blind_input_file(width, height, parity, poserr, codetol, distractors,
                          xyfile, matchfile, rdlsfile, wcsfile, canfile,
                          totaltime, totalcpu,
                          solved, depths)

    log('Image size: %d x %d' % (width, height))
    log('Parity:', parity)
    log('Poserr:', poserr)
    if matchfile:
        log('Match file:', matchfile)
    if rdlsfile:
        log('RDLS file:', rdlsfile)
    if wcsfile:
        log('WCS file:', wcsfile)
    if canfile:
        log('Cancel file:', canfile)

    input_file = []
    def blind_add(*x)
        input_file.append(' '.join((str(y) for y in x)))

    blind_add('total_timelimit', totaltime)
    blind_add('total_cpulimit', totalcpu)
    blind_add()
    stripe = 0
    for d in range(len(depths) - 1):
        for ful, fuu, inds in scales:
            for i in inds:
                blind_add('index', i)
            blind_add('field',             xyfile)
            blind_add('fieldw',            width)
            blind_add('fieldh',            height)
            blind_add('sdepth',            depths[d])
            blind_add('depth',             depths[d+1])
            blind_add('parity',            parity)
            blind_add('fieldunits_lower',  ful)
            blind_add('fieldunits_upper',  fuu)
            blind_add('tol',               codetol)
            blind_add('distractors',       distractors)
            blind_add('verify_pix',        poserr)
            blind_add('ratio_toprint       1e3')
            blind_add('ratio_tokeep        1e9')
            blind_add('ratio_tosolve       1e9')
            blind_add('ratio_tobail        1e-100')
            if tweak:
                blind_add('tweak')
                blind_add('tweak_aborder',  tweakorder)
                blind_add('tweak_abporder', tweakorder)
                blind_add('tweak_skipshift')
            if matchfile:
                blind_add('match', '%s-%s'%(matchfile, stripe))
            if rdlsfile:
                blind_add('indexrdls', rdlsfile)
                blind_add('indexrdls_solvedonly')
            if wcsfile:
                blind_add('wcs', '%s-%s'%(wcsfile, stripe))
            if canfile:
                blind_add('cancel', canfile)
            blind_add('solved', solved)
            blind_add('fields 0')
            blind_add('run')
            stripe += 1

    return '\n'.join(input_file)

    # run blind

    # if solved:
    #   merge rdls  files
    #   merge match files


    # move to frontend:
    # if solved:
    #   wcs-xy2rd field.xy.fits
    #   rdlsinfo field.rd.fits
    #   sort match file by logodds
    #   wcs-rd2xy index.rd.fits
    #   add AN_JOBID

    # Success
    return 0

def main():
    from optparse import OptionParser
    parser = OptionParser()
    parser.add_option("-i", "--infile",
                      dest="infile",
                      help="input image FILE", metavar="FILE")

    (options, args) = parser.parse_args()

    if not options.infile:
        parser.error('required argument missing: infile')

    return solve_field(options.infile)

if __name__ == '__main__':
    sys.exit(main())
