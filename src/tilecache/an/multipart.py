#! /usr/bin/env python

import re
import sys

def log(x):
    print x

CRLF = '\r\n'
ALPHANUM = r'A-Za-z0-9_\-'
class multipart:
    fin = None
    # unparsed data
    data = ''
    boundary = None
    contentlength = 0
    #endheader = False
    inparthdr = False
    formparts = []
    fileform = 'file'
    tempfile = None

    def __init__(self, fin):
        self.fin = fin

    def readmore(self):
        newdata = self.fin.read(4096)
        self.data += newdata
        matched = False
        log('read %d bytes; now have %d.' % (len(newdata), len(self.data)))
        log('data starts with:\n\n***%s***\n' % self.data[0:64])
        if not self.boundary:
            log('looking for content-type...')
            ctre = re.compile(r'^Content-Type: multipart/form-data; boundary=(?P<boundary>[' + ALPHANUM + r']+)' + CRLF)
            res = ctre.match(self.data)
            if not res:
                log('no match.')
            if res:
                log('matched: "%s"' % self.data[res.start(0):res.end(0)])
                self.boundary = res.group('boundary')
                log('boundary: "%s"' % self.boundary)
                end = res.end(0)
                self.data = self.data[end:]
                matched = True
        if not self.contentlength:
            log('looking for content-length...')
            ctre = re.compile(r'^Content-Length: (?P<length>[0-9]+)' + CRLF)
            res = ctre.match(self.data)
            if not res:
                log('no match.')
            if res:
                log('matched: "%s"' % self.data[res.start(0):res.end(0)])
                self.contentlength = int(res.group('length'))
                log('length: "%d"' % self.contentlength)
                end = res.end(0)
                self.data = self.data[end:]
                matched = True

        #if not self.endheader:
        #    log('looking for end of header')
        #    ctre = re.compile(r'^\r\n')
        #    res = ctre.match(self.data)
        #    if not res:
        #        log('no match.')
        #    if res:
        #        log('matched: "%s"' % self.data[res.start(0):res.end(0)])
        #        self.endheader = True
        #        end = res.end(0)
        #        self.data = self.data[end:]
        #        matched = True
        if self.boundary:
            log('looking for boundary\n  (--%s)' % self.boundary)
            ctre = re.compile(CRLF + '--' + self.boundary + r'(--)?' + CRLF)
            res = ctre.match(self.data)
            if not res:
                log('no match.')
            if res:
                log('matched: "%s"' % self.data[res.start(0):res.end(0)])
                end = res.end(0)
                self.data = self.data[end:]
                self.inparthdr = True
                matched = True
        
        if self.inparthdr:
            log('looking for content-disposition')
            ctre = re.compile(r'^Content-Disposition: form-data; name="(?P<name>[' + ALPHANUM + r']+)"' +
                              r'(; filename="(?P<filename>[' + ALPHANUM + r']+)")?' + CRLF)
            res = ctre.match(self.data)

            # HACK
            maintype = None
            subtype = None
            fieldname = None
            filename = None

            if not res:
                log('no match.')
            if res:
                log('matched: "%s"' % self.data[res.start(0):res.end(0)])
                fieldname = res.group('name')
                filename = res.group('filename')
                log('field: %s' % fieldname)
                end = res.end(0)
                self.data = self.data[end:]
                matched = True
            log('looking for content-type')
            ctre = re.compile(r'^Content-Type: (?P<type>[' + ALPHANUM + r']+)/(?P<subtype>[' + ALPHANUM + r']+)' + CRLF)
            res = ctre.match(self.data)
            if not res:
                log('no match.')
            if res:
                log('matched: "%s"' % self.data[res.start(0):res.end(0)])
                maintype = res.group('type')
                subtype = res.group('subtype')
                end = res.end(0)
                self.data = self.data[end:]
                matched = True
            
            log('looking for end of part header.')
            if self.data.startswith(CRLF):
                log('found it.')
                self.data = self.data[2:]
                self.inparthdr = False
                self.inbody = True
                self.formparts.append({'field':fieldname, 'data':'', 'type':maintype, 'subtype':subtype})
                return

        if self.inbody:
            log('in body.')
            if not self.data.startswith(CRLF):
                # append start of data to this part's data.
                i = self.data.find(CRLF)
                if (i == -1):
                    # no CRLF, append the whole thing.
                    log('chomping:\n***%s***' % self.data)
                    (self.formparts[-1])['data'] += self.data
                    self.data = ''
                else:
                    log('chomping:\n***%s***' % self.data[0:i])
                    (self.formparts[-1])['data'] += self.data[0:i]
                    self.data = self.data[i:]

            if len(self.data) > 2:
                log('looking for boundary')
                ctre = re.compile(CRLF + '--' + self.boundary + r'(--)?' + CRLF)
                res = ctre.match(self.data)
                if not res:
                    log('no match.')
                if res:
                    log('matched: "%s"' % self.data[res.start(0):res.end(0)])
                    end = res.end(0)
                    self.data = self.data[end:]
                    self.inparthdr = True
                    self.inbody = False
                    matched = True
                else:
                    log('no match.  Eating the CRLF.')
                    (self.formparts[-1])['data'] += self.data[0:2]
                    self.data = self.data[2:]


if __name__ == '__main__':
    mp = multipart(sys.stdin)
    while True:
        mp.readmore()
