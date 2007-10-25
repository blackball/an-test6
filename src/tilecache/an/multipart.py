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

    def get_part_body_state(self, mainbody):
        return PartBodyState(self, mainbody)

    def get_body_state(self):
        ct = self.mainheader.headers['Content-Type']
        if ct.startswith('multipart/form-data'):
            return self.get_multipart_body_state(ct)
        else:
            log('Content-type is "%s", not multipart/form-data.' % ct)
        return None

    def get_multipart_body_state(self, ct):
        return MultipartBodyState(self, ct)

    def main_header_done(self):
        log('main header finished.')
        for k,v in self.mainheader.headers.items():
            log('  ' + str(k) + " = " + str(v))
        self.body = self.get_body_state()
        self.state = self.body
        if not self.state:
            log('No body state found.')
            self.error = True

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
        # call up to the parent so that subclassers only have to
        # override one class...
        #PartBodyState(self.machine, self)
        return self.machine.get_part_body_state(self)

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
        if not self.currentpart:
            log('Preamble ended.')
        else:
            datalen = self.bodyparser.get_data_length()
            data = self.bodyparser.get_data()
            log('Body ended.  Got %d bytes of data.' % datalen)
            log('Data is: ***%s***' % data)
            self.currentpart['data'] = data
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
                self.last_part_body_done()
            else:
                self.part_body_done()
            return

        # not found.  chomp the data up to the last CR, if it
        # exists, or the whole string otherwise.
        ind = stream.data.rfind(CR)
        if (ind == -1):
            # not found; chomp the whole string.
            ind = len(stream.data)
        self.handle_data(stream.data[:ind])
        stream.discard_data(ind)

    def get_data_length(self):
        return len(self.data)

    def get_data(self):
        return self.data

    def part_body_done(self):
        self.body.part_body_done()

    def last_part_body_done(self):
        self.body.last_part_body_done()

    def handle_data(self, data):
        log('PartBodyState: handling data ***%s***' % data)
        self.data += data

    def reset_data(self):
        self.data = ''




class FileMultipart(Multipart):
    writefields = {}

    def get_part_body_state(self, mainbody):
        superme = super(FileMultipart, self)
        currentpart = mainbody.currentpart
        if not currentpart:
            return superme.get_part_body_state(mainbody)

        key = 'field'
        fnkey = 'filename'
        if not ((key in currentpart) and \
                (fnkey in currentpart)):
            return superme.get_part_body_state(mainbody)

        fn = currentpart[fnkey]
        field = currentpart[key]
        filename = self.get_filename(field, fn, currentpart)
        if not filename:
            return superme.get_part_body_state(mainbody)
        return FileBodyState(self, mainbody, filename)

    def get_filename(self, field, filename, currentpart):
        if not field in self.writefields:
            return None
        return self.writefields[field]

class FileBodyState(PartBodyState):
    filename = None
    fid = None
    datalen = 0

    def __init__(self, machine, mainbody, filename):
        self.filename = filename
        self.fid = open(filename, 'wb')
        super(FileBodyState, self).__init__(machine, mainbody)
        self.data = None

    def handle_data(self, data):
        self.datalen += len(data)
        self.fid.write(data)

    def get_data_length(self):
        return self.datalen

    def part_body_done(self):
        self.fid.close()
        self.fid = None
        super(FileBodyState, self).part_body_done()

if __name__ == '__main__':
    #mp = Multipart(sys.stdin)
    mp = FileMultipart(sys.stdin)
    mp.writefields['file'] = '/tmp/contents-file'
    #mp.blocksize = 1
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

