CC = gcc
LFLAGS = -lm
                
#fast
#CFLAGS	= -O2 -Wall -DAMFAST -D__USE_FIXED_PROTOTYPES__ -DUNIX_TTY_PLATFORM

#debug
CFLAGS	= -g -O0 -Wall -D__USE_FIXED_PROTOTYPES__ -DUNIX_TTY_PLATFORM

OBJ = starutil.o KD/command.o KD/amar.o KD/ambs.o KD/amdyv_array.o KD/amdyv.o KD/amma.o KD/amiv.o KD/am_file.o KD/am_proc.o KD/am_string.o KD/am_string_array.o KD/am_time.o KD/distances.o KD/genarray.o KD/hrect.o KD/kdtree.o KD/knn.o KD/kquery.o KD/kresult.o KD/kresults.o KD/neighbors.o KD/rangesearch.o

STAROBJ = star.o
RCOBJ = randcat.o
CTOBJ = codetree.o
GQOBJ = getquads.o
GFOBJ = genfields.o
SLOBJ = solvexy.o
PQOBJ = pixquads.o


all: randcat codetree getquads genfields solvexy pixquads

randcat: $(OBJ) $(RCOBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) $(RCOBJ) -o $@

codetree: $(OBJ) $(CTOBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) $(CTOBJ) -o $@

getquads: $(OBJ) $(GQOBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) $(GQOBJ) -o $@

genfields: $(OBJ) $(GFOBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) $(GFOBJ) -o $@

solvexy: $(OBJ) $(SLOBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) $(SLOBJ) -o $@

pixquads: $(OBJ) $(PQOBJ)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJ) $(PQOBJ) -o $@

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

allclean:
	rm -f KD/*.o *.o randcat codetree getquads genfields solvexy pixquads *~

clean:
	rm -f randcat codetree getquads genfields solvexy pixquads randcat.o codetree.o getquads.o genfields.o solvexy.o pixquads.o starutil.o

