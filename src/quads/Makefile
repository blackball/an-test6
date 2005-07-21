.PHONY: all allclean clean

CC = gcc
LFLAGS = -lm
                
#fast
#CFLAGS	= -O2 -Wall -DAMFAST -D__USE_FIXED_PROTOTYPES__ -DUNIX_TTY_PLATFORM

#debug
CFLAGS	= -g -O0 -Wall -D__USE_FIXED_PROTOTYPES__ -DUNIX_TTY_PLATFORM

OBJ = starutil.o KD/command.o KD/amar.o KD/ambs.o KD/amdyv_array.o KD/amdyv.o KD/amma.o KD/amiv.o KD/am_file.o KD/am_proc.o KD/am_string.o KD/am_string_array.o KD/am_time.o KD/distances.o KD/genarray.o KD/hrect.o KD/kdtree.o KD/knn.o KD/kquery.o KD/kresult.o KD/kresults.o KD/neighbors.o KD/rangesearch.o

EXECS = randcat startree codetree getquads genfields solvexy pixquads
MYOBJS = randcat.o startree.o codetree.o getquads.o genfields.o solvexy.o pixquads.o starutil.o

all: $(EXECS)

$(EXECS) :: %: %.o $(OBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) $< -o $@

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

allclean:
	rm -f KD/*.o *.o $(EXECS) *~

clean:
	rm -f $(EXECS) $(MYOBJS)

