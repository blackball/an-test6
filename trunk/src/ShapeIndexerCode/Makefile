
# general macros
COMPILER       = g++
FLAGS          = --no-warnings -fpermissive -IGInclude -Wno-deprecated
INDEX_EXEC     = StarIndexer
SOLVER_EXEC    = StarSolver
RATIO_EXEC     = RatioLister
CBUILDER_EXEC  = CatBuilder
CCONVERT_EXEC  = CatConverter
TCONVERT_EXEC  = TychoConverter
IMBUILDER_EXEC = ImBuilder
SFINDER_EXEC   = ShapeFinder
SIMGEN_EXEC    = SingleImGen

EXECS          = ${INDEX_EXEC} ${SOLVER_EXEC} ${RATIO_EXEC} ${CBUILDER_EXEC} ${CCONVERT_EXEC} ${TCONVERT_EXEC} ${IMBUILDER_EXEC} ${SFINDER_EXEC} ${SIMGEN_EXEC}

# header directories
HMAIN_DIR     = .
HEXTRA_DIR    = ./GInclude

# source directories
SMAIN_DIR     = .
SEXTRA_DIR    = ./GSource

# objects
STAR_OBJS    = mainShapeIndexer.o mainShapeSolver.o mainRatioLister.o mainCatBuilder.o mainCatConverter.o mainConvertTycho.o mainImBuilder.o mainShapeFinder.o mainSingleImGen.o
MAIN_OBJS    = StarImage.o StarList.o StarShapeSys.o StarCamera.o StarKDTree.o Tycho2Reader.o
TOOL_OBJS    = BlockMemBank.o MachinePort.o StringExt.o MathExt.o HashTable.o BaseTimer.o 

# libs
LIBS         = -lm

# executable compilation
all:  ${EXECS}

${INDEX_EXEC}: mainShapeIndexer.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainShapeIndexer.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${INDEX_EXEC} ${FLAGS} ${LIBS}

${SOLVER_EXEC}: mainShapeSolver.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainShapeSolver.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${SOLVER_EXEC} ${FLAGS} ${LIBS}

${RATIO_EXEC}: mainRatioLister.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainRatioLister.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${RATIO_EXEC} ${FLAGS} ${LIBS}

${CBUILDER_EXEC}: mainCatBuilder.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainCatBuilder.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${CBUILDER_EXEC} ${FLAGS} ${LIBS}

${CCONVERT_EXEC}: mainCatConverter.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainCatConverter.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${CCONVERT_EXEC} ${FLAGS} ${LIBS}

${TCONVERT_EXEC}: mainConvertTycho.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainConvertTycho.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${TCONVERT_EXEC} ${FLAGS} ${LIBS}
	
${IMBUILDER_EXEC}: mainImBuilder.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainImBuilder.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${IMBUILDER_EXEC} ${FLAGS} ${LIBS}

${SFINDER_EXEC}: mainShapeFinder.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainShapeFinder.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${SFINDER_EXEC} ${FLAGS} ${LIBS}

${SIMGEN_EXEC}: mainSingleImGen.o ${MAIN_OBJS} ${TOOL_OBJS}
	${COMPILER} mainSingleImGen.o ${MAIN_OBJS} ${TOOL_OBJS} -o ${SIMGEN_EXEC} ${FLAGS} ${LIBS}

# main objects
mainShapeIndexer.o: ${HEXTRA_DIR}/MyTypes.h ${HMAIN_DIR}/StarList.hpp ${HMAIN_DIR}/StarShapeSys.hpp ${SMAIN_DIR}/mainShapeIndexer.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainShapeIndexer.cpp -o mainShapeIndexer.o

mainShapeSolver.o: ${HEXTRA_DIR}/MyTypes.h ${HMAIN_DIR}/StarImage.hpp ${HMAIN_DIR}/StarList.hpp ${HMAIN_DIR}/StarShapeSys.hpp ${SMAIN_DIR}/mainShapeSolver.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainShapeSolver.cpp -o mainShapeSolver.o

mainRatioLister.o: ${HEXTRA_DIR}/MyTypes.h ${HMAIN_DIR}/StarShapeSys.hpp ${SMAIN_DIR}/mainRatioLister.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainRatioLister.cpp -o mainRatioLister.o

mainCatBuilder.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/MathExt.h ${HMAIN_DIR}/StarCamera.hpp ${HMAIN_DIR}/StarList.hpp ${SMAIN_DIR}/mainCatBuilder.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainCatBuilder.cpp -o mainCatBuilder.o

mainCatConverter.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/BlockMemBank.hpp ${HMAIN_DIR}/StarCamera.hpp ${HMAIN_DIR}/StarList.hpp ${SMAIN_DIR}/mainCatConverter.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainCatConverter.cpp -o mainCatConverter.o

mainConvertTycho.o: ${HEXTRA_DIR}/MyTypes.h ${HMAIN_DIR}/Tycho2Reader.hpp ${HMAIN_DIR}/StarList.hpp ${SMAIN_DIR}/mainConvertTycho.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainConvertTycho.cpp -o mainConvertTycho.o

mainImBuilder.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/MathExt.h ${HEXTRA_DIR}/StringExt.h ${HMAIN_DIR}/StarCamera.hpp ${HMAIN_DIR}/StarImage.hpp ${HMAIN_DIR}/StarList.hpp ${HMAIN_DIR}/StarShapeSys.hpp ${SMAIN_DIR}/mainImBuilder.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainImBuilder.cpp -o mainImBuilder.o

mainShapeFinder.o: ${HEXTRA_DIR}/MyTypes.h ${HMAIN_DIR}/StarShapeSys.hpp ${SMAIN_DIR}/mainShapeFinder.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainShapeFinder.cpp -o mainShapeFinder.o

mainSingleImGen.o: ${HEXTRA_DIR}/MyTypes.h ${HMAIN_DIR}/StarImage.hpp ${HMAIN_DIR}/StarCamera.hpp ${HMAIN_DIR}/StarList.hpp ${HMAIN_DIR}/StarShapeSys.hpp ${SMAIN_DIR}/mainSingleImGen.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/mainSingleImGen.cpp -o mainSingleImGen.o

StarImage.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/MachinePort.h ${HMAIN_DIR}/StarImage.hpp ${SMAIN_DIR}/StarImage.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/StarImage.cpp -o StarImage.o

StarList.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/MachinePort.h ${HMAIN_DIR}/StarList.hpp ${SMAIN_DIR}/StarList.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/StarList.cpp -o StarList.o

StarShapeSys.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/MathExt.h ${HEXTRA_DIR}/BlockMemBank.hpp ${HEXTRA_DIR}/MachinePort.h ${HMAIN_DIR}/StarImage.hpp ${HMAIN_DIR}/StarKDTree.hpp ${HMAIN_DIR}/StarList.hpp ${HMAIN_DIR}/StarShapeSys.hpp ${SMAIN_DIR}/StarShapeSys.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/StarShapeSys.cpp -o StarShapeSys.o

StarCamera.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/MathExt.h ${HEXTRA_DIR}/BlockMemBank.hpp ${HEXTRA_DIR}/MachinePort.h ${HMAIN_DIR}/StarImage.hpp ${HMAIN_DIR}/StarList.hpp ${HMAIN_DIR}/StarCamera.hpp ${SMAIN_DIR}/StarCamera.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/StarCamera.cpp -o StarCamera.o

StarKDTree.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/BlockMemBank.hpp ${HMAIN_DIR}/StarKDTree.hpp ${SMAIN_DIR}/StarKDTree.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/StarKDTree.cpp -o StarKDTree.o

Tycho2Reader.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/BlockMemBank.hpp ${HMAIN_DIR}/Tycho2Reader.hpp ${SMAIN_DIR}/Tycho2Reader.cpp
	${COMPILER} ${FLAGS} -c ${SMAIN_DIR}/Tycho2Reader.cpp -o Tycho2Reader.o

# tool objects
BlockMemBank.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/Enumeration.hpp ${HEXTRA_DIR}/BlockMemBank.hpp ${SEXTRA_DIR}/BlockMemBank.cpp
	${COMPILER} ${FLAGS} -c ${SEXTRA_DIR}/BlockMemBank.cpp -o BlockMemBank.o

MachinePort.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/MachinePort.h ${SEXTRA_DIR}/MachinePort.cpp
	${COMPILER} ${FLAGS} -c ${SEXTRA_DIR}/MachinePort.cpp -o MachinePort.o

MathExt.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/BlockMemBank.hpp ${HEXTRA_DIR}/MathExt.h ${SEXTRA_DIR}/MathExt.cpp
	${COMPILER} ${FLAGS} -c ${SEXTRA_DIR}/MathExt.cpp -o MathExt.o

StringExt.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/StringExt.h ${SEXTRA_DIR}/StringExt.cpp
	${COMPILER} ${FLAGS} -c ${SEXTRA_DIR}/StringExt.cpp -o StringExt.o

HashTable.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/Enumeration.hpp ${HEXTRA_DIR}/BlockMemBank.hpp ${HEXTRA_DIR}/HashTable.hpp ${SEXTRA_DIR}/HashTable.cpp
	${COMPILER} ${FLAGS} -c ${SEXTRA_DIR}/HashTable.cpp -o HashTable.o
	
BaseTimer.o: ${HEXTRA_DIR}/MyTypes.h ${HEXTRA_DIR}/BaseTimer.hpp ${SEXTRA_DIR}/BaseTimer.cpp
	${COMPILER} ${FLAGS} -c ${SEXTRA_DIR}/BaseTimer.cpp -o BaseTimer.o

# cleanup
clean:
	rm -f ${EXECS} ${STAR_OBJS} ${MAIN_OBJS} ${TOOL_OBJS}
