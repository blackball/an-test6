Two main programs are used for solving star images. The first is StarIndexer, which indexes a star catalog and builds several indexes containing shapes constrained by different angles and different numbers of stars-per-shape. The second is StarSolver which takes a list of indexes, a catalog, and a list of images, and attempts to use shapes in the images to determine the view angle and orientation of the images.

Before we can use the indexer or solver however, we require two things: a star catalog, and a set of images to solve. The star catalog required by StarIndexer and StarSolver is in a binary format that is portable to either big or little-endian machines. The following programs can be used to build and manipulate these catalog files:
_______________________________________________________________________________

1. CatBuilder
=============

CatBuilder builds a catalog of uniformly distributed stars and saves the result in a file readable by StarIndexer and StarSolver.

Usage: CatBuilder <catalog_name> <star_count>
<cat_name>	: Name of the file to output the catalog into.
<star_count>	: The number of stars to generate

Ex: CatBuilder cat.rand.10000.bin 10000

2. CatConverter
===============

CatConverter converts a list of stars in a text file into a binary file readable by StarIndexer and StarSolver. Each line in the text file should be in the form:

<id> <x> <y> <z>

Ex: 
------catalog.text-----
0 1 0 0 
1 0 1 0
-----------------------

where <x> <y> and <z> represent the 3D position of the star on the unit sphere. The <id> value is an integer denoting the ID of the star, however in practice CatConverter simply ignores this value.

Usage: CatConverter <text_cat_name> <bin_cat_name>
<text_cat_name>	: The name of the input text file.
<bin_cat_name> 	: The name of the output file to create.

Ex: CatConverter catalog.text catalog.bin

3. TychoConverter
=================

TychoConverter converts the Tycho2 star catalog into a binary file readable by StarIndexer and StarSolver.

Usage: TychoConverter

The converter uses several parameters that are defined at the top of the file ‘mainConvertTycho.cpp’ that indicate the path of the Tycho2 catalog file, the path of the Tycho2 index file, and the final output file name.
_______________________________________________________________________________

The images used by StarSolver are in an ASCII format where each line in the file is of the form:

<id> <x> <y>

<id> is the ID (i.e. the order) of the star in the original catalog. So the first star in the catalog would have an ID in a image file of ‘1’. An ID of ‘0’ is used to indicate stars whose ID is unknown or who are not in the original catalog. The values <x> and <y> refer to the x/y-position of the center of the star within the image. These should be in the range [-1,1] x [-1,1].

Ex:
------image.txt-----
0 	0.25 	0.75
98	0.5	-0.6
--------------------


 The following programs can be used to build images:
_______________________________________________________________________________

1. ImBuilder
============

ImBuilder builds a set of images from a star catalog using random orientations, and random view angles within user-defined constraints. Distractor stars can be added, a percentage of catalog stars can be dropped out, and the positions of the stars can be jittered. The user provides a prefix for the name of all the generated image files, and the image files are named using this prefix followed by an ID number. A file if created with a name in the form ‘<prefix>_image.dat’ which contains the camera basis and view angle of every generate image. A final file is also constructed with a name in the form ‘<prefix>_image.lst’, which contains the name of all the generated image files.

Usage: ImBuilder <im_count> <prefix> <dest_dir> <cat_file> <dist_file> 
		     <view_angle_min> <view_angle_max> <drop-out rate> <jitter>
<im_count>		: The number of random images to generate.
<prefix>		: The prefix to be used for the name of each image file.
<dest_dir>		: The destination directory for all files.
<cat_file>		: The catalog file to generated the images from.
<dist_file>		: A catalog of stars to draw distractor stars from.
<view_angle_min>	: The minimum view angle to use in the images.
<view_angle_max>	: The maximum view angle to use in the images.
<drop-out rate> 	: The percentage of catalog stars to drop out of the images.
<jitter>		: The positional jitter to apply to the stars, in [0,1].

Ex: ImBuilder 100 im_ TempDat catalog2.bin dist1.000.bin 1.0 1.4 0 0

2. SingleImGen
==============

SingleImGen generates a single image from a star catalog at a position defined by the user using a random orientation.

Usage: SingleImGen <catalog> <dist_cat> <image_output> <RA> <DEC> 
		          <view_angle> <drop-out rate> <jitter>
<catalog>		: The catalog to generate the image from.
<dist_cat>		: The catalog to draw distractor stars from.
<image_output>	: The file to write the image to.
<RA>			: The Right Ascension of the image to generate.
<DEC>			: The Declination of the image to generate.
<view_angle>		: The view angle of the image to generate.
<drop-out rate> 	: The percentage of catalog stars to drop out of the images.
<jitter>		: The positional jitter to apply to the stars, in [0,1].

Ex: SingleImGen cat.part1.bin dist1.000.bin im.tychopart1.txt 0 90 0.5 0 0
_______________________________________________________________________________

Given a star catalog, we can now use StarIndexer to generate shapes from the catalog. 

StarIndexer
===========

StarIndexer Builds a set of indices according to the parameters in a user-provided parameter file and a user-provided star catalog. In the parameter file several pieces of information are used which impact the resulting indexes. The process proceeds as follows: given a variety of wedge sizes and numbers of stars-per-shape, a separate index is created for every combination of wedge angle and stars-per-shape. For each star in the catalog, all the stars within a user-specified view angle are found, and shapes are searched for in order from the least probable to most probable (ex: a wedge angle of 3 degrees is checked before a wedge angle of 90 degrees). For a particular index all the shapes discovered around a single star are saved, and if the total number of shapes assigned to that star is above a user-supplied threshold no more indexing occurs for the star. If, on the other hand, fewer than the threshold number of shapes has been found, indexing proceeds with the next-least probable set of parameters. It should be noted that the view angle used to index the catalog should always be smaller than that of the images it will be asked to solve, since test images are unlikely to be perfectly centered on an indexed star. That is, given test images with a view angle of, say, 0.75 degree, it would useful to use indexes constructed with a view angle of 0.5 degrees. The resulting shapes are organized in a table according to the distance of three stars in the shape. The shapes are organized into bins containing shapes with very similar distance values.

Usage: StarIndexer <build_param_file> <cat_file>
<build_param_file>	: The file containing the index building parameters
<cat_file>		: catalog file for indexing

Ex: StarIndexer build.params cat.rand.10000.bin

The parameter file should contain:
<index_file_base>	: The base name for all resulting index files.
<im_angle>		: The image view angle
<max_shapes>		: The maximum shapes per star to record.
<bincount>		: The number of bins to use in each index.
<wedge_angle_cnt>	: The number of different wedge angles to use.
<wedge_angles>+	: The <wedge_angle_cnt> wedge angles.
<stars_p_shape_cnt>	: The number of different stars per shape to use.
<stars_p_shape>+	: The <stars_p_shape_cnt> stars per shape

Ex:
------build.params-----
shapes.tycho.
0.5
6
100000
8
22.5 25.7143 30.0 36.0 45.0 60.0 90.0 180.0
4
9 7 6 4
-----------------------

To explain: the file builds a set of indexes prefixed by ‘shapes.tycho’, using a view angle of 0.5 degrees. Six shapes are used as the cutoff number of shapes to record for each star. 100000 bins are used in the index file for looking up shapes. Eight different wedges sizes, (22.5, 25.7143, 30.0, 36.0, 45.0, 60.0, 90.0, and 180.0 degrees) are used to find shapes. Four different numbers of stars per shapes are used, (9, 7, 6, and 4) when finding shapes.


Once a set of indexes has been produced, preferably using several different view angles, images can be solved using the StarSolver.
_______________________________________________________________________________

StarSolver
==========

Given a list of image names (like that generated by ImBuilder) and a list of indexes (like those generated by StarIndexer), the program reads each image and then attempts to solve the image using a list of indexes. The image is tested against each index in order from largest to smallest indexed view angle.

Usage: StarSolver <output_results> <cat_file> <index_file> <name_file>
<output_results>	: The output file for results.
<cat_file>		: The star catalog file to match image stars to.
<index_desc>		: A file listing the indexes to use in solving along with 
          tolerance to use when matching shapes.
<name_file>		: A file containing a list of star images to solve. The 
			  images are in the ASCII form described previously.

Ex: StarSolver out.txt cat.rand.10000.bin index.lst im__image.lst

The file <index_desc> contains lines of the form:
<index_name> <min_angle_tol> <max_angle_tol> <min_distance_tol> <max_distance_tol>

<index_name>		: Refers to the name of an index to use.
<min_angle_tol>	: Refers to the smallest tolerance to use when matching 
			  angles in shapes.
<max_angle_tol>	: Refers to the largest tolerance to use when matching 
			  angles in shapes.
<min_distance_tol>	: Refers to the smallest tolerance to use when matching 
			  distances in shapes.
<max_distance_tol>	: Refers to the largest tolerance to use when matching 
			  distances in shapes.

Ex: index.shapes 0.01 0.1 0.0001 0.001
_______________________________________________________________________________

Finally, there remain several programs that can be used to verify and test the content of the index files for the purpose of interest, or to help debug the indexer and solver when changes have been made.

1. RatioLister
==============

Outputs the distance ratios and angles of all the shapes in an index into 
text files. This is useful for checking the distribution of values in the index.

Usage: RatioLister <index_file> <ratio_file> <angle_file>
<index_file>	: The index file to open.
<ratio_file>	: The file to output ratios into.
<angle_file>	: The file to output angles into.

Ex: RatioLister index.tycho.1.bin ratios.txt angles.txt

The <ratio_file> is of the form:

<first_count> <remaining_count>
<ratios>+

<first_count>: Refers to the number of ratios in the file that are ‘first’
         ratios, i.e. the ratios that are used to hash into the index 
         table to search for shapes. The first <first_count> ratios in the 
         file are all ‘first’ ratios.
<remaining_count>: Refers to the ‘remaining’ ratios, i.e. those distance ratios 
		      in each shape that are *not* used to hash into the table. 
              After the first <first_count> ratios, the remaining ratios 
              in the file are all ‘remaining’ ratios.
<ratios>: Refers to all the ratios in the ASCII file, all of which are in [0,1]

Ex:
------ratios.text-----
1 2
0.5
0.78
0.88
----------------------

The <angle_file> is of the form:

<angle_count>
<angles>+

<angle_count>: Refers to the number of angles that follow in the file.
<angles>: Refers to the angle, all of which are measured relative to the orienting line of the shape, and so are all in [-180,180].

Ex:
------angles.text-----
2
12.7
14
----------------------

2. ShapeFinder
==============

Outputs information about all the shapes in an index file that a particular star acts as the center for.

Usage: <index_file> <star_num>
<index_file>	: The index file to search.
<star_num>	: The star ID to find shapes for.

Output is to the console, and for each shape returned in of the form:

Shape <shape_id>
Center: <star_num>   Far: <far_star>   Third: <third_star>
Ratios:
<ratios>
Angles:
<angles>

<shape_id>: Refers to the ID of the shape in question.
<star_num>: Should be the same as the user-parameter <star_num>.
<far_star>: Refers to the ID of the star used to define the distance ratios in 
	     the shape.
<third_star>: Refers to the ID of a third star in the shape, (currently always 
		the nearest star to the shape’s center star).
<ratios>: Refers to all the distance ratios in the shape.
<angles>: Refers to all the angles in the shape.

Ex:
----------------------
Shape 98
Center: 103   Far: 42   Third: 1056
Ratios:
0.2 0.56
Angles:
0.0 26.0
----------------------
_______________________________________________________________________________

Examples
========

For to purpose of clarity, below are several examples of using the above programs.

Example 1: In this example, we create a uniform 10000-star catalog and 0-star distractor catalog. The star catalog is indexed, 100 test images are built, and the images are then solved. The indexer uses the parameter file ‘build.params’ which contains:

------build.params-----
shapes.tycho.
15
6
10000
8
22.5 25.7143 30.0 36.0 45.0 60.0 90.0 180.0
4
9 7 6 4
----------------------

The solver uses a list containing the names of all the indexes, which is of the form:

------index.lst------
shapes.tycho.0.bin 0.01 0.1 0.0001 0.001
shapes.tycho.1.bin 0.01 0.1 0.0001 0.001
.
.
.
---------------------

Finally, the command sequence would be:

> CatBuilder cat.rand.100000.bin 10000
> CatBuilder cat.distractors.0.bin 0
> StarIndexer build.params cat.rand.10000.bin
> ImBuilder 100 im ./ cat.rand.10000.bin cat.distractors.0.bin 0.75 0.9 0 0
> StarSolver results.txt cat.rand.10000.bin index.lst im_image.lst

Example 2: In this example, a similar process is followed, this time creating an index using the Tycho2 star catalog, and trying to solve a single image. Note that since the Tycho2 catalog contains approx. 2.5 million stars this example will require on the order of a couple days to complete. TychoConverter has been modified to produce the output catalog ‘cat.tycho.bin’. The solver uses an image list named ‘im.lst’ which contains:

------im.lst------
im.txt
------------------

The command sequence would therefore be:

> TychoConverter
> CatBuilder cat.distractors.0.bin 0
> StarIndexer build.params cat.tycho.bin
> SingleImGen .tycho.bin cat.distractors.0.bin im.txt 0 90 0.85 0 0
> StarSolver results.txt cat.tycho.bin index.lst im.lst

