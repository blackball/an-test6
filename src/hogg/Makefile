CC= gcc
LDFLAGS= -lm
RM= /bin/rm -fv
TOUCH= touch

all: hoggTestPassed

TWEAK= hoggTweak.h hoggMath.h hoggLogsum.o hoggLogGaussian.o hoggExMaxStep.o \
	hoggLeastSquareFit.o
TEST= hoggTest.o hoggTestLogsum.o hoggTestLogGaussian.o hoggTestExMaxStep.o \
	hoggTestLeastSquareFit.o

hoggTest: $(TWEAK) $(TEST)
	$(RM) ./hoggTestPassed
	$(CC) $(TWEAK) $(TEST) -o hoggTest $(LDFLAGS)

hoggTestPassed: hoggTest
	./hoggTest
	$(TOUCH) ./hoggTestPassed
