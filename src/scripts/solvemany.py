import os
import sys
import glob
import time

import numpy
import scipy
import pyfits
from pysqlite2 import dbapi2 as sqlite
from PIL import Image, ImageOps

sys.path.append('../simplexy')
sys.path.append('../quads')
sys.path.append('../ontheweb/execs')
import image2pnm
import fits2xy

DBNAME="solvemany.sqlite"

SCHEMA="""
CREATE TABLE solves (
    filename, solved, solvedate
);
"""
default_config = (
 ('ANAPPL',   0.1,       'scale estimate: arcseconds per pixel, lower bound'),
 ('ANAPPU',   180*60*60, 'scale estimate: arcseconds per pixel, upper bound'),
 ('ANPARITY', 'BOTH',     '"POS" / "NEG" / "BOTH", default "BOTH"'),
 ('ANPOSERR', 1,         'positional error, in pixels'),
 ('ANTWEAK',  0,         '0 / 1 - whether to run tweak'),
 ('ANTWEAKO', 2,         'tweak polynomial order'),  
 ('ANUNAME',  'solvemany.py',             "user's name"),
 ('ANEMAIL',  'solvemany@astrometry.net', "users's email"),
 ('ANINSRC',  'file',    'input file origin: img file, url, fits file, etc'),
)

def connect_db():
    create_table = not os.path.exists(DBNAME)
    con = sqlite.connect(database=DBNAME, timeout=10.0)
    cur = con.cursor()
    if create_table:
        cur.execute(SCHEMA)
    return con, cur

join = os.path.join
def solve_field(file, cur):

    solvedir = '%s-SOLVE' % file
    os.mkdir(solvedir)
    pnmfile = join(solvedir,'image.pnm')
    image2pnm.convert_image(file,
                            pnmfile,
                            join(solvedir,'image.uncompressed'),
                            join(solvedir,'image.sanitized.fits'))

    color_im = Image.open(pnmfile)
    grayscale_im = ImageOps.grayscale(color_im)
    data = scipy.fromimage(grayscale_im)
    data = data.astype(numpy.float32)
    data = numpy.array(data, numpy.float32)
    print data.shape

    # Do the actual source extraction and get a FITS table
    tbhdu = fits2xy.source_extract(data)

    h = tbhdu.header
    h.add_comment('Parameters used for the blind solver')
    for key, val, comment in default_config:
        h.update(key, val, comment)

    outfile = pyfits.HDUList()
    outfile.append(pyfits.PrimaryHDU()) 
    outfile.append(tbhdu)
    outfile.writeto(join(solvedir, 'image.xy.fits'))

    # DO THE SOLVE JAZZ
    solved = 1
    
    # do stuff, solve it -- store 0 or 1 in solved depending
    cur.execute("INSERT INTO solves VALUES (?, ?, ?)",
                (file, solved, time.ctime()))

def main():
    con, cur = connect_db()

    images = glob.glob('images/*')
    for image in images:
        solve_field(image, cur)
    
    con.commit()

if __name__ == '__main__':
    main()
