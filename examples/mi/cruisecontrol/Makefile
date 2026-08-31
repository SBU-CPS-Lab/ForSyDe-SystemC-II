EXTRACFLAGS = 
EXTRA_LIBS = -lmigdb

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../Makefile.defs

CFLAGS += -DFORSYDE_INTROSPECTION -DFORSYDE_COSIMULATION_WRAPPERS

