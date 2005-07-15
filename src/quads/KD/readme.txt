
simkd - simple kd-tree construction and query
---------------------------------------------

The simkd application constructs a kd-tree from the contents of a data
file (e.g., example-data.csv) of k-dimensional vectors, and then performs
nearest neighbor searches within the kd-tree using query points from the
query file (e.g., example-queries.csv).  The search can be either for K
nearest neighbors, or for all neighbors within some range (radius) of the
query point.  (Annoying note: the k's in kd-tree and k-nearest neighbor are
not the same.)  The dimensions of all data points and query points must be
the same, or the program will die horribly and yell at you.

The output of the program is a file (output.csv by default) with
lines of the form:

    query_record,rank,data_record

where:

    query_record:    the record number of the current query point in
                     the query file (starting from 0)

    data_record:     the record number of a data point in the data file
                     that is "close to" the current query point (from 0)

    rank:            the "closeness rank" of the data_record point to
                     the query_record point: 0 is closest, 1 is next
                     closest, and so on.  (If taskname is knn and k is 1
                     [the defaults], then rank will necessarily be 0 in
                     each output line.)

For example, this line in the output file,

    3,1,321

means that, for the 4th query point, the 2nd closest data point is the
322nd point in the data file.  (*Please note* that, if your data
file contains a header line and/or blank lines, the 332nd point
will *not* be described by the 332nd line in the data file.  Ditto
for the query file.)


Command line arguments:
-----------------------

  Required:

    data <filename>        example: example-data.csv

                                This file contains a sequence of vectors
                                in comma-separated value format
                                representing the points from which the
                                kd-tree will be built, and against which
                                nearest neighbor queries will be performed.
                                Can be 2-dimensional, as the example file,
                                or greater-than-2-dimensional, e.g.:

                                      w,      x,      y,      z

                                0.50000,0.00000,0.10000,0.12500
                                2.00000,2.00000,2.50000,0.00000
                                1.12500,1.00000,0.00000,3.50000
                                             ...

                                Optionally, *the first line only* can
                                contain field names, for your own
                                convenience (e.g., "w, x, y, z").  Blank
                                lines in the file are ignored.

    query <filename>        example: example-queries.csv

                                This file contains a sequence of vectors
                                in comma-separated value format
                                representing the points for which we want
                                to locate nearest neighbors from the data
                                file.  (The dimension of the query
                                vector(s) must match the dimension
                                of the data vectors.)

                                Optionally, *the first line only* can
                                contain field names, for your own
                                convenience (e.g., "w, x, y, z").  Blank
                                lines in the file are ignored.

   *Note* that it is fine to use the *same* file as both the data file
   and the query file in order, for example, to find the 5 nearest
   neighbors to each point within a single data set.

  Optional:

    out <filename>          default: output.csv   Output file name.
                                                  *See above for content*

    taskname <string>       default: knn    Task to perform, one of:
                                            knn - find K nearest neighbors
                                            rangesearch - find points in range
                                             (and eventually: rangecount -
                                             count points in range)

    k <int>                 default: 1      Only relevant if taskname == knn.
                                            The number of nearest neighbors to
                                            find for each query point.

    range <double>          default: 10     Only relevant if taskname != knn.
                                            The range (or radius) around each
                                            query point in which to search for
                                            nearest neighbors.  The range is in
                                            the same units as those used for
                                            the data and query points.  A very
                                            narrow range may find no data
                                            points "near"--within range of--
                                            a given query point; a very wide
                                            range may find that *all* data
                                            points are "near" a given query
                                            point.

    rmin <int>              default: 50     The maximum number of points to
                                            store in a leaf node of the
                                            kd-tree: higher values may
                                            increase speed of kd-tree
                                            construction at cost of slower
                                            search, and vice versa.  Must
                                            have rmin >= 1.

    method <string>         default: singletree

                                        Can only by singletree right now.
                                        Refers to the algorithm for 
                                        doing the task.  (Later we hope
                                        to include dualtree.)

Example Command Lines:
----------------------

$ ./simkd data example-data.csv query example-queries.csv

    Constructs a kd-tree with the four 2-dimensional vectors from file
    "example-data.csv", then performs the four 1-nearest neighbor
    queries for the points from file "example-queries.csv".  The output
    will be in file "output.csv".  (An ASCII-art diagram of the data and
    query points for this example is in file "example.txt".)

$ ./simkd data your-d.csv query your-q.csv k 5 rmin 20 out your-out.csv

    Constructs a kd-tree with the M k-dimensional vectors from
    "your-d.csv", then performs the N 5-nearest neighbor queries for
    the points from "your-q.csv".  The output is "your-out.csv".
    Since rmin is 20, there will be at most 20 data point per leaf of
    the kd-tree, meaning that kd-tree construction will be "slower"
    but queries will be "faster" than for the default rmin of 50.
    (Depending on the size of the data set, the size of the query set,
    and the amount of real memory on your machine, memory management may
    dominate array traversals or vice versa: your mileage may vary.)

$ ./simkd data ex2-d.csv query ex2-q.csv taskname rangesearch range 500

    Constructs a kd-tree from the one thousand 4-dimensional
    vectors in the "ex2-d.csv" file, then performs the fifty
    queries from the "ex2-q.csv" file, identifying all data points within
    500 units of each query point.  You'll notice in the output file
    ("output.csv" by default) that most query points have no data points
    within the range/radius of 500 units, a few have one data point
    within range, and a few have multiple data points within range.

$ ./simkd range 50 query ex2-q.csv taskname rangesearch data ex2-d.csv

    Same as previous.  The order of the argument pairs does not matter, as
    long as each pair is constructed correctly.
