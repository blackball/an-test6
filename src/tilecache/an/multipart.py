#! /usr/bin/env python

import re
import sys

def log(x):
    print x

class StreamParser(object):
    fin = None
    # unparsed data
    data = ''
    blocksize = 4096
    error = False
    needmore = False

    def __init__(self, fin=None):
        self.fin = fin

    # Returns False if EOF is reached or an error occurs
    def readmore(self):
        newdata = self.fin.read(self.blocksize)
        if len(newdata) == 0:
            return False
        return self.moreinput(newdata)

    # Returns False if a parsing error has occurred
    def moreinput(self, newdata):
        self.data += newdata
        self.needmore = False
        while not (self.error or self.needmore):
            self.process()
        return not self.error

    # Sets self.error if a parsing error occurs;
    # sets self.needmore if more data is needed to proceed.
    def process(self):
        pass

    def discard_data(self, nchars):
        self.data = self.data[nchars:]


class StateMachine(StreamParser):
    state = None

    def __init__(self, fin=None):
        super(StateMachine, self).__init__(fin)
        self.state = self.initial_state()

    def initial_state(self):
        return None

    def set_state(self, newstate):
        self.state = newstate

    def process(self):
        if not self.state:
            log('Error: no state')
            self.error = True
            return
        self.state.process()

class StateMachineState(object):
    machine = None
    def __init__(self, machine=None):
        self.machine = machine
    def process(self):
        pass


CRLF = '\r\n'
CR = '\r'
LWSP = ' \t'
ALPHANUM = r'A-Za-z0-9_\-'

class Multipart(StateMachine):
    mainheader = None
    body = None
    parts = []

    def __init__(self, fin=None):
        self.mainheader = MainHeaderState(self)
        super(Multipart, self).__init__(fin)

    def initial_state(self):
        return self.mainheader

    def add_part(self, part):
        self.parts.append(part)

    def main_header_done(self):
        log('main header finished.')
        for k,v in self.mainheader.headers.items():
            log('  ' + str(k) + " = " + str(v))

        ct = self.mainheader.headers['Content-Type']
        if ct.startswith('multipart/form-data'):
            self.body = MultipartBodyState(self, ct)
            self.state = self.body
        else:
            log('Content-type is "%s", not multipart/form-data.' % ct)
            self.error

class HeaderState(StateMachineState):
    headers = None
    def process(self):
        stream = self.machine
        if len(stream.data) < 2:
            stream.needmore = True
            return
        ind = stream.data.find(CRLF)
        if (ind == -1):
            stream.needmore = True
            return
        if (ind == 0):
            #stream.discard_data(2)
            self.header_done()
            return
        self.process_line(ind)
        stream.discard_data(ind+2)

    def header_done(self):
        pass
    def process_line(self, lineend):
        pass

class MainHeaderState(HeaderState):
    def __init__(self, machine=None):
        super(MainHeaderState, self).__init__(machine)
        self.headers = {}
    def header_done(self):
        if not 'Content-Type' in self.headers:
            log('No Content-Type.')
            self.machine.error = True
        self.machine.main_header_done()
    def process_line(self, lineend):
        stream = self.machine
        line = stream.data[:lineend]
        linere = re.compile(r'^' +
                            '(?P<name>[' + ALPHANUM + r']+)' +
                            r':' +
                            r'[' + LWSP + r']+' +
                            r'(?P<value>[' + LWSP + ';/="' + ALPHANUM + r']+)' +
                            '$')
        match = linere.match(line)
        if not match:
            log('MainHeaderState: no match for line "%s"' % line)
            stream.error = True
            return
        log('matched: "%s"' % stream.data[match.start(0):match.end(0)])
        self.process_header(match.group('name'), match.group('value'))

    def process_header(self, name, value):
        self.headers[name] = value


class MultipartBodyState(StateMachineState):
    inheader = True
    boundary = None
    headerparser = None
    bodyparser = None
    currentpart = None

    def __init__(self, machine, ct):
        ctre = re.compile(r'^multipart/form-data; boundary=(?P<boundary>[' + ALPHANUM + r']+)$')
        res = ctre.match(ct)
        if not res:
            log('no boundary found')
            raise ValueError, 'boundary not found in Content-Type string.'
        self.boundary = res.group('boundary')
        log('boundary: "%s"' % self.boundary)
        super(MultipartBodyState, self).__init__(machine)
        self.inheader = False
        self.bodyparser = self.get_body_parser()

    def get_header_parser(self):
        if self.headerparser:
            self.headerparser.reset_headers()
            return self.headerparser
        return PartHeaderState(self.machine, self)

    def get_body_parser(self):
        return PartBodyState(self.machine, self)

    def process(self):
        stream = self.machine
        #log('MultipartBodyState: parsing "%s"...' % stream.data[:64])
        if self.inheader:
            self.headerparser.process()
        else:
            self.bodyparser.process()

    def part_header_done(self):
        headers = self.headerparser.headers

        self.inheader = False
        self.currentpart = {}
        self.currentpart['headers'] = headers

        key = 'Content-Disposition'
        if key in headers:
            cd = headers[key]
            cdre = re.compile(r'^form-data; name="(?P<name>[' + ALPHANUM + r']+)"' +
                              r'(; filename="(?P<filename>[' + ALPHANUM + r']+)")?$')
            match = cdre.match(cd)
            if match:
                field = match.group('name')
                filename = match.group('filename')
                if field:
                    self.currentpart['field'] = field
                    self.currentpart['filename'] = field
        key = 'Content-Type'
        if key in headers:
            ct = headers[key]
            self.currentpart['content-type'] = ct

        self.machine.add_part(self.currentpart)
        self.bodyparser = self.get_body_parser()

    def part_body_done(self):
        if not self.currentpart: #len(self.parts) == 0:
            log('Preamble ended.')
        else:
            log('Body ended.  Got %d bytes of data.' % len(self.bodyparser.data))
            log('Data is: ***%s***' % self.bodyparser.data)
            self.currentpart['data'] = self.bodyparser.data
        self.headerparser = self.get_header_parser()
        self.inheader = True

    def last_part_body_done(self):
        log('Last body ended.')
        self.part_body_done()

class PartHeaderState(MainHeaderState):
    body = None
    def __init__(self, machine, body):
        super(PartHeaderState, self).__init__(machine)
        self.body = body
    def header_done(self):
        log('part header finished.')
        # chomp the CRLF.
        self.machine.discard_data(2)
        for k,v in self.headers.items():
            log('  ' + str(k) + " = " + str(v))
        self.body.part_header_done()
    def reset_headers(self):
        self.headers = {}

class PartBodyState(StateMachineState):
    body = None
    data = None
    def __init__(self, machine, body):
        super(PartBodyState, self).__init__(machine)
        self.body = body
        self.reset_data()
    def process(self):
        stream = self.machine
        bdy = self.body.boundary
        if (len(stream.data) < len(bdy) + 8):
            stream.needmore = True
            return
        bdyre = re.compile(CRLF + '--' + bdy + r'(?P<dashdash>--)?' + CRLF)
        match = bdyre.search(stream.data)
        if match:
            # got it!
            start = match.start(0)
            relen = match.end(0) - start
            if (start > 0):
                # chomp some data off the front first.
                self.handle_data(stream.data[:start])
                stream.discard_data(start)

            stream.discard_data(relen)
            if match.group('dashdash'):
                # it's the end of the last part.
                self.body.last_part_body_done()
            else:
                self.body.part_body_done()
            return

        # not found.  chomp the data up to the last CR, if it
        # exists, or the whole string otherwise.
        ind = stream.data.rfind(CR)
        if (ind == -1):
            # not found; chomp the whole string.
            ind = len(stream.data)
        self.handle_data(stream.data[:ind])
        stream.discard_data(ind)

    def handle_data(self, data):
        log('PartBodyState: handling data ***%s***' % data)
        self.data += data

    def reset_data(self):
        self.data = ''


#class MainBodyState(StateMachineState):
#    def process(self):
#        stream = self.machine
#        stream.error = True



class multipart:
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
            log('looking for boundary\n      (--%s)' % self.boundary)
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
    mp = Multipart(sys.stdin)
    while mp.readmore():
        pass
    if mp.error:
        print 'Parser failed.'
    else:
        print 'Parser succeeded'
        for p in mp.parts:
            print '  Part:'
            for k,v in p.items():
                print '    ', k, '=', v

