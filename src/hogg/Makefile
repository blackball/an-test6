CC= gcc
LDFLAGS= -lm

all: hoggTest

TWEAK= hoggTweak.h hoggMath.h hoggLogsum.o hoggLogGaussian.o
TEST= hoggTest.o hoggTestLogsum.o hoggTestLogGaussian.o

hoggTest: $(TWEAK) $(TEST)
	$(CC) $(TWEAK) $(TEST) -o hoggTest $(LDFLAGS)
