CC= gcc
LDFLAGS= -lm

all: hoggTest

TWEAK= hoggTweak.h hoggMath.h hoggLogsum.o hoggLogGaussian.o hoggExMaxStep.o
TEST= hoggTest.o hoggTestLogsum.o hoggTestLogGaussian.o hoggTestExMaxStep.o

hoggTest: $(TWEAK) $(TEST)
	$(CC) $(TWEAK) $(TEST) -o hoggTest $(LDFLAGS)
	./hoggTest
