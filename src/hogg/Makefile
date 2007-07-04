# This file is part of the Astrometry.net suite.
# Copyright 2007 David W. Hogg.
#
# The Astrometry.net suite is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, version 2.
#
# The Astrometry.net suite is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Astrometry.net suite ; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

all: hoggTestPassed

BASEDIR := ..
COMMON := $(BASEDIR)/an-common
include $(COMMON)/makefile.gsl

CC= gcc
LDFLAGS= -lm
RM= /bin/rm -fv
TOUCH= touch

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
