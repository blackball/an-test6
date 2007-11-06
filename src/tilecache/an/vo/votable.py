from an.vo.log import log


class xmlelement(object):
    #etype = None
    #args = {}
    #children = []

    def __str__(self):
        #log('str(): %s' % self.etype)
        s = ('<%s' % str(self.etype))
        for k,v in self.args.items():
            s += (' %s="%s"' % (k, v))
        if len(self.children):
            s += '>\n'
            #log('%i children.' % len(self.children))
            for c in self.children:
                #log('%s child:' % self.etype)
                s += str(c) + '\n'
            s += ('</%s>' % str(self.etype))
        else:
            s += ' />'
        return s

    def __init__(self):
        self.children = []
        self.args = {}
        self.etype = None

    def add_child(self, x):
        #log('adding child to %s' % self.etype)
        self.children.append(x)

class VOTableDocument(xmlelement):
    xml_header = r'<?xml version="1.0"?>'

    def __init__(self):
        super(VOTableDocument, self).__init__()
        self.etype = 'VOTABLE'
        self.args.update({
            'version' : '1.1',
            'xmlns:xsi' : 'http://www.w3.org/2001/XMLSchema-instance',
            'xsi:noNamespaceSchemaLocation' : 'http://www.ivoa.net/xml/VOTable/VOTable/v1.1',
            })

    def __str__(self):
        s = self.xml_header + '\n' + super(VOTableDocument, self).__str__()
        return s



class VOResource(xmlelement):
    def __init__(self, name=None):
        super(VOResource, self).__init__()
        self.etype = 'RESOURCE'
        if name:
            self.args['name'] = name

class VOTable(xmlelement):
    def __init__(self, name=None):
        super(VOTable, self).__init__()
        self.etype = 'TABLE'
        if name:
            self.args['name'] = name

class VOField(xmlelement):
    def __init__(self, name, datatype, arraysize=None, ucd=None):
        super(VOField, self).__init__()
        self.etype = 'FIELD'
        self.args['name'] = name
        self.args['datatype'] = datatype
        if arraysize:
            self.args['arraysize'] = arraysize
        if ucd:
            self.args['ucd'] = ucd
    
class VOInfo(xmlelement):
    def __init__(self, name=None):
        super(VOInfo, self).__init__()
        self.etype = 'INFO'
        if name:
            self.args['name'] = name
