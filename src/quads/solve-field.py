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




paritymap = { 'POS' : 0, 'NEG' : 1, 'BOTH' : 2 }

def log(*x):
    print >> sys.stderr, 'solve-field:', ' '.join(x)

def die(*x):
    print >> sys.stderr, 'solve-field:', ' '.join(x)
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
        #die('Failed to read FITS input file %s' % xylist)
    return solve_field(xyfile, fitsin)

def solve_field(xyfile, xyfits):
    #for v in indexes:
        #ql = v['quadl']
        #qu = v['quadu']
        #paths = v['paths']
        #print ql, qu
        #for p in paths:
        #print p

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
    #log hdrcards
    #log
    
    W = hdr['IMAGEW']
    H = hdr['IMAGEH']

    paritystr = get_val_nothrow(hdr, 'ANPARITY')
    if (not paritystr) or (not paritystr in paritymap):
        paritystr = 'BOTH'
    parity = paritymap[paritystr]

    poserr = get_val_nothrow(hdr, 'ANPOSERR')
    if (not poserr) or (float(poserr) <= 0):
        poserr = 1.0

    matchfile = get_val_nothrow(hdr, 'ANMATCH')
    rdlsfile  = get_val_nothrow(hdr, 'ANRDLS')
    wcsfile   = get_val_nothrow(hdr, 'ANWCS')
    canfile   = get_val_nothrow(hdr, 'ANCANCEL')

    tweak     = get_val_nothrow(hdr, 'ANTWEAK')
    dotweak = (tweak == 'T')
    if dotweak:
        tweako    = get_val_nothrow(hdr, 'ANTWEAKO')
        if tweako:
            tweakorder = int(tweako)

    scales = []
    i = 0
    while True:
        keyl = 'ANAPPL%d' % i
        keyu = 'ANAPPU%d' % i
        #log keyl, keyu
        ful = get_val_nothrow(hdr, keyl)
        fuu = get_val_nothrow(hdr, keyu)
        if not(ful and fuu):
            break
        #log ful, fuu
        ful = float(ful)
        fuu = float(fuu)
        #log ful, fuu
        if (ful == 0 or fuu == 0):
            break
        # Estimate size of quads we could find:
        fmax = 1.0 * max(W, H) * fuu / 60.0;
        fmin = 0.1 * min(W, H) * ful / 60.0;

        # Collect indices with overlapping scales:
        inds = []
        for v in indexes:
            ql = v['quadl']
            qu = v['quadu']
            if (not((fmin > qu) or (fmax < ql))):
                inds += v['paths']

        if len(inds) == 0:
            # bigger than the biggest?
            v = indexes[0]
            if (fmin > v['quadu']):
                inds += v['paths']
            # smaller than the smallest?
            v = indexes[len(indexes)-1];
            if (fmax < v['quadl']):
                inds += v['paths']

        scales.append({ 'ful':ful, 'fuu':fuu, 'indices':inds})
        i+=1


    log 'Image size: %d x %d' % (W, H)
    log 'Parity: %d' % parity
    log 'Poserr: %f' % poserr

    if matchfile:
        log 'Match file: ' + matchfile
    if rdlsfile:
        log 'RDLS file: ' + rdlsfile
    if wcsfile:
        log 'WCS file: ' + wcsfile
    if canfile:
        log 'Cancel file: ' + canfile

    #for s in scales:
    #    ful = s['ful']
    #    fuu = s['fuu']
    #    inds = s['indices']
    #    log 'Scale: %f to %f' % (ful, fuu)
    #    for i in inds:
    #        log '  ', i

    instr = "total_timelimit " + str(totaltime) + "\n" + \
            "total_cpulimit " + str(totalcpu) + "\n\n"
    stripe = 0
    for d in range(len(depths) - 1):
        startd = depths[d]
        endd = depths[d+1]

        for s in scales:
            ful = s['ful']
            fuu = s['fuu']
            inds = s['indices']

            for i in inds:
                instr += "index " + i + "\n"

            instr += "field " + xyfile + "\n" + \
                     "fieldw " + str(W) + "\n" + \
                     "fieldh " + str(H) + "\n" + \
                     "sdepth " + str(startd) + "\n" + \
                     "depth " + str(endd) + "\n" + \
                     "parity " + str(parity) + "\n" + \
                     "fieldunits_lower " + str(ful) + "\n" + \
                     "fieldunits_upper " + str(fuu) + "\n" + \
                     "tol " + str(codetol) + "\n" + \
                     "distractors " + str(distractors) + "\n" + \
                     "verify_pix " + str(poserr) + "\n" + \
                     "ratio_toprint 1e3\n" + \
                     "ratio_tokeep 1e9\n" + \
                     "ratio_tosolve 1e9\n" + \
                     "ratio_tobail 1e-100\n"

            if dotweak:
                instr += "tweak\n"
                if tweakorder:
                    instr += "tweak_aborder " + str(tweakorder) + "\n" + \
                             "tweak_abporder " + str(tweakorder) + "\n" + \
                             "tweak_skipshift" + "\n"

            if matchfile:
                instr += "match " + matchfile + "-" + str(stripe) + "\n"
            if rdlsfile:
                instr += "indexrdls " + rdlsfile + "-" + str(stripe) + "\n"
            if wcsfile:
                instr += "wcs " + wcsfile + "-" + str(stripe) + "\n"
            if canfile:
                instr += "cancel " + canfile + "\n"

            instr += "solved " + solved + "\n"
            instr += "fields 0" + "\n"
            instr += "run" + "\n\n"
            stripe += 1

    print instr

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
