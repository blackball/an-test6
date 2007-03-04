#usage example: ./parse.py -fposi Possi.boundaries > Possi.boundaries.merc 

"""
Parsers for survey files containing 4 data points in x,y,z space obtained by dwf
"""
from starutil import *
import sys

MDIST = 0.05 #.001

def unit_vector(arr):
	mag = norm(arr)
	return map(lambda x: x/mag, arr)

def norm(arr):
    return sqrt(sum(map(lambda x: x**2, arr)))

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
    max_dist = MDIST
    supersampled = []
    
    for line in sfile:
        tokens = line.split()
        corners = []
        
        for i in range(0, len(tokens), 3):
        	corners += [(float(tokens[i]), float(tokens[i+1]), float(tokens[i+2]))]	# coalate every 3 numbers into a tuple
        fields += [corners]	# fields is a list of lists, each list containing typles representing the corners of a field
    
    # supersample lines
    for field in fields:
    	expanded = []	# expand each boundary line by supersampling it. for eech field, return list of supersampled points
    	for i in range(len(field)): # for each corner point
    		p1 = field[i]
    		p2 = field[(i+1) % len(field)]
    		vector = map(lambda x, y: x-y, p2, p1)	# vector to get from p1 to p2
    		vlength = norm(vector)
    		n_inter_points = int(ceil(vlength / float(max_dist)))
    		hopsize = vlength / n_inter_points
    		direction = map(lambda x: x/vlength, vector)	# unit vector in the direction p1 to p2
    		
    		# get the supersampled points in range [p1, p2) and store in expanded
    		expanded.append(p1)
    		for j in range(0, n_inter_points - 1):
    			curpoint = map(lambda x, y: x+y, p1, map(lambda x: (j + 1) * hopsize * x, direction)) # add (j+1)*hopsize chunks of direction to p1.
    			curpoint = unit_vector(curpoint)
    			expanded.append(curpoint) 
    		
    	expanded.append(field[0]) # add first point at the end
    	supersampled += [expanded]
        
        '''
        p1 = (xy2ra(float(tokens[0]), float(tokens[1])), z2dec(float(tokens[2])))
        p2 = (xy2ra(float(tokens[3]), float(tokens[4])), z2dec(float(tokens[5])))
        p3 = (xy2ra(float(tokens[6]), float(tokens[7])), z2dec(float(tokens[8])))
        p4 = (xy2ra(float(tokens[9]), float(tokens[10])), z2dec(float(tokens[11])))
        ''
        p1 = (ra2mercx(xy2ra(float(tokens[0]), float(tokens[1]))), dec2mercy(z2dec(float(tokens[2]))))
        p2 = (ra2mercx(xy2ra(float(tokens[3]), float(tokens[4]))), dec2mercy(z2dec(float(tokens[5]))))
        p3 = (ra2mercx(xy2ra(float(tokens[6]), float(tokens[7]))), dec2mercy(z2dec(float(tokens[8]))))
        p4 = (ra2mercx(xy2ra(float(tokens[9]), float(tokens[10]))), dec2mercy(z2dec(float(tokens[11]))))
        
        
        fields += [[p1, p2, p3, p4]]
        '''
        
        # at this point supersampled contains a list of supersampled tuples, each list represents one field.
        # the tuples are in x,y,z format. We need to convert them to mercator-x,y coordinates
#         result = []
    for field in supersampled:
#         # for each field, insert a corresponding list in result, and then fill that field with mercator-x,y 
#         # tuples, one tuple for each corresponding x,y,z tuple in field
#         	result.append([])
    	for point in field:
#         		result[-1].append((ra2mercx(xy2ra(float(point[0]), float(point[1]))), dec2mercy(z2dec(float(point[2])))))
    		print str(ra2mercx(xy2ra(float(point[0]), float(point[1]))))
    		print str(dec2mercy(z2dec(float(point[2]))))
    	print("")
        
    return

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

#     try:
#     	int(opts["-n"])
#     except:
#     	print "number of points to sample (n) must be an int"
#     	sys.exit(1)
        
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
    # field1.pointn.y
    # <empty line>
    # field2.point1.x
    # ...
    # field2.pointn.y
    # <empty line>
    # ...
#     for field in fields:
#         for point in field:
#             for coord in point:
#                  print str(coord)
#         print("")
        
        
        #coords = reduce(lambda x, y: list(x) + list(y), record)
        #print "\t".join(map(str, coords))