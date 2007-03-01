#usage example: ./parse.py -fposi Possi.boundaries > Possi.boundaries.merc 

"""
Parsers for survey files containing 4 data points in x,y,z space obtained by dwf
"""
from starutil import *
import sys

def parse_posi(sfile):
    """
    Parse up the PosI Survey data. Get 4 points in x,y,z space
    Convert to RA-Dec format and then to mercator x,y
    returns a list of (x,y) points i.e. a list of list of tuples
    
    TODO: directly convert from x,y,z to merc-x,y? instead of
    (x,y,z) -> (ra, dec) -> (merc-x, merc-y)
    
    TODO: maybe return ra-dec co-ordinates and do some other massaging
    before converting to mercator co-ordintaes (like normalizing?, supersampling?)
    """
    fields = []
    for line in sfile:
        tokens = line.split()
        '''
        p1 = (xy2ra(float(tokens[0]), float(tokens[1])), z2dec(float(tokens[2])))
        p2 = (xy2ra(float(tokens[3]), float(tokens[4])), z2dec(float(tokens[5])))
        p3 = (xy2ra(float(tokens[6]), float(tokens[7])), z2dec(float(tokens[8])))
        p4 = (xy2ra(float(tokens[9]), float(tokens[10])), z2dec(float(tokens[11])))
        '''
        p1 = (ra2mercx(xy2ra(float(tokens[0]), float(tokens[1]))), dec2mercy(z2dec(float(tokens[2]))))
        p2 = (ra2mercx(xy2ra(float(tokens[3]), float(tokens[4]))), dec2mercy(z2dec(float(tokens[5]))))
        p3 = (ra2mercx(xy2ra(float(tokens[6]), float(tokens[7]))), dec2mercy(z2dec(float(tokens[8]))))
        p4 = (ra2mercx(xy2ra(float(tokens[9]), float(tokens[10]))), dec2mercy(z2dec(float(tokens[11]))))
        
        
        fields += [[p1, p2, p3, p4]]
    return fields

## usage: parse.py -fposi Possi.boundaries
formats = {'posi':parse_posi} #, \
#'posii':parse_posii, 'south': parse_south, '2mass':parse_twomass, 'parse_aaor':parse_aaor }

if __name__ == "__main__":
    import getopt
    try:
        opts, args = getopt.getopt(sys.argv[1:], "f:")
    except getopt.GetoptError:
        print "No format specified."
        sys.exit(1)   
    opts = dict(opts)
    if "-f" not in opts:
        print "No format."
        sys.exit(1)

    elif opts["-f"] not in formats:
        print "Invalid format"
        sys.exit(1)
    elif len(args) == 0:
        print "No input file specified"
        sys.exit(1)

    parser = formats[opts["-f"]]
    f = file(args[0], "r")
    fields = parser(f)
    f.close()
    
    # format of output is:
    # field1.point1.x
    # field1.point1.y
    # field1.point2.x
    # field1.point3.y
    # ...
    # field1.point4.y
    # <empty line>
    # field2.point1.x
    # ...
    # field2.point4.y
    # <empty line>
    # ...
    for field in fields:
        for point in field:
            for coord in point:
                print str(coord)
        print("")
        
        
        #coords = reduce(lambda x, y: list(x) + list(y), record)
        #print "\t".join(map(str, coords))