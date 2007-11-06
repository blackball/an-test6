import logging
import re

from django import newforms as forms
from django.db import models
from django.http import HttpResponse, HttpResponseRedirect
from django.newforms import widgets, ValidationError, form_for_model
from django.template import Context, RequestContext, loader

#from an.portal.models import Job, AstroField, UserPreferences

from an import gmaps_config
from an import settings
from an.vo.log import log
from an.vo.votable import *

#intersects = [ 'COVERS', 'ENCLOSED', 'CENTER', 'OVERLAPS' ]

def siap_pointed(request):

    res = HttpResponse()
    res['Content-Type'] = 'text/xml'

    doc = VOTableDocument()
    resource = VOResource()
    resource.args['type'] = 'result'
    doc.add_child(resource)
    qstatus = VOInfo('QUERY_STATUS')
    resource.add_child(qstatus)

    try:
        pos_str = request.GET['POS']
        pos = map(float, pos_str.split(','))
        if len(pos) != 2:
            raise ValueError('POS must contain two values (RA and Dec); got %i' % len(pos))

        size_str = request.GET['SIZE']
        size = map(float, size_str.split(','))
        if not (len(size) in [1, 2]):
            raise ValueError('SIZE must contain one or two values; got %i' % len(size))

        #intr = None
        #if 'INTERSECT' in request.GET:
        #    intr = request.GET['INTERSECT']
        #    if not (intr in intersects):
        #        intr = None
        #if not intr:
        #    intr = 'OVERLAPS'

        # For now, ignore INTERSECT, NAXIS, CFRAME, EQUINOX, CRPIX,
        # CRVAL, CDELT, ROTANG, PROJ, VERB

        formats = []
        if 'FORMAT' in request.GET:
            fre = re.compile(
                r'(?P<clause>ALL|GRAPHIC|METADATA|'
                r'image/(png|fits|jpeg|gif)|'
                r'text/html|'
                r'GRAPHIC-(ALL|(jpeg|png|gif)(,(jpeg|png|gif)){0,2}$)'
                r')(,.*|$)')
            fmt = request.GET['FORMAT']

            while len(fmt):
                m = fre.match(fmt)
                if not m:
                    raise ValueError('Unrecognized FORMAT clause "%s"' % fmt)
                #log('matched "%s"' % fmt[m.start('clause'):m.end('clause')])
                formats.append(fmt[m.start('clause'):m.end('clause')])
                fmt = fmt[m.end('clause'):]
                if len(fmt) and fmt[0] == ',':
                    fmt = fmt[1:]


        else:
            formats.append('ALL')


        log('POS:', pos, 'SIZE:', size)
        log('FORMATS:', formats)

        qstatus.args['value'] = 'OK'

        table = VOTable('results')
        resource.add_child(table)

        # a short (usually one line) description of the image
        # identifying the image source (e.g., survey name), object name or field coordinates,
        # bandpass/filter, and so forth.
        image_title = VOField('image_title', 'char', '*', 'VOX:Image_Title')
        table.add_child(image_title)
        # MUST

        # the instrument or instruments used to make the observation, e.g., STScI.HST.WFPC2.
        instrument = VOField('instrument', 'char', '*', 'INST_ID')
        table.add_child(instrument)
        # SHOULD

        # the mean modified Julian date of the observation.
        # By "mean" we mean the midpoint of the observation in terms of normalized exposure times:
        # this is the "characteristic observation time" and is independent of observation duration.
        date = VOField('jdate', 'double', None, 'VOX:Image_MJDateObs')
        table.add_child(date)
        # SHOULD

        # the ICRS right-ascension of the center of the image.
        ra = VOField('RA_center', 'double', None, 'POS_EQ_RA_MAIN')
        table.add_child(ra)
        # MUST

        # the ICRS declination of the center of the image.
        dec = VOField('DEC_center', 'double', None, 'POS_EQ_DEC_MAIN')
        table.add_child(dec)
        # MUST

        # the number of image axes.
        naxes = VOField('naxes', 'int', None, 'VOX:Image_Naxes')
        table.add_child(naxes)
        # MUST

        # the length in pixels of each image axis.
        naxis = VOField('naxis', 'int', '*', 'VOX:Image_Naxis')
        table.add_child(naxis)
        # MUST

        # the scale in degrees per pixel of each image axis.
        scale = VOField('scale', 'double', '*', 'VOX:Image_Scale')
        table.add_child(scale)
        # MUST

        # the MIME-type of the object associated with the image acref, e.g., "image/fits", "text/html", and so forth.
        format = VOField('format', 'char', '*', 'VOX:Image_Format')
        table.add_child(format)
        # MUST

        # the coordinate system reference frame, selected from "ICRS", "FK5", "FK4", "ECL", "GAL", and "SGAL".
        refframe = VOField('refframe', 'char', '*', 'VOX:STC_CoordRefFrame')
        table.add_child(refframe)
        # SHOULD
        
        # the Equinox (not required for ICRS) of the coordinate system used for the image world coordinate system (WCS).
        # This should match whatever is in the image WCS and may differ from the default ICRS coordinates used elsewhere.
        equinox = VOField('equinox', 'double', None, 'VOX:STC_CoordEquinox')
        table.add_child(equinox)
        # MAY

        # the three-character code ("TAN", "ARC", "SIN", and so forth) specifying the celestial projection, as for FITS WCS.
        proj = VOField('proj', 'char', '3', 'VOX:STC_CoordProjection')
        table.add_child(proj)
        # SHOULD

        # the image pixel coordinates of the WCS reference pixel. This is identical to "CRPIX" in FITS WCS.
        crpix = VOField('crpix', 'double', '*', 'VOX:WCS_CoordRefPixel')
        table.add_child(crpix)
        # SHOULD

        # the world coordinates of the WCS reference pixel. This is identical to "CRVAL" in FITS WCS.
        crval = VOField('crval', 'double', '*', 'VOX:WCS_CoordRefValue')
        table.add_child(crval)
        # SHOULD

        # the WCS CD matrix. This is identical to the "CD" term in FITS WCS, and defines the scale and rotation
        # (among other things) of the image.
        # Matrix elements should be ordered as CD[i,j] = [1,1], [1,2], [2,1], [2,2].
        cd = VOField('cd', 'double', '*', 'VOX:WCS_CDMatrix')
        table.add_child(cd)
        # SHOULD

        # specifying the URL to be used to access or retrieve the image.
        # Since the URL will often contain metacharacters the URL is normally enclosed in an XML CDATA section
        # (<![CDATA[...]]>) or otherwise encoded to escape any embedded metacharacters.
        image_url = VOField('url', 'char', '*', 'VOX:Image_AccessReference')
        table.add_child(image_url)
        # MUST

        # the minimum time to live in seconds of the access reference.
        image_ttl = VOField('ttl', 'int', None, 'VOX:Image_AccessRefTTL')
        table.add_child(image_ttl)
        # MAY

        # the actual or estimated size of the encoded image in bytes (not pixels!). This is useful for image selection
        # and for optimizing distributed computations.
        image_filesize = VOField('filesize', 'int', None, 'VOX:Image_FileSize')
        table.add_child(image_filesize)
        # SHOULD

        '''
   5. Constant Valued FIELDs. If a FIELD of the table will have the same value for every row of the table it is permissible to represent the field as a PARAM of the resource rather than a table FIELD. Both FIELDs and PARAMs can be used to define image metadata: a FIELD allows a different value to be specified for every image, whereas PARAM allows metadata to be defined globally for all images. The same attribute tags (UCD, datatype, arraysize, type="trigger", etc.) must be defined in either case.
'''
        
    except (KeyError, ValueError),e:
        qstatus.args['value'] = 'ERROR'
        qstatus.add_child(str(e))
        log('error:', e)

    res.write(str(doc))
    return res

def getimage(request):
    return HttpResponse('not implemented')

