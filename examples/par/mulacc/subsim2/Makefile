EXTRACFLAGS = 
EXTRA_LIBS =

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../../Makefile.defs

# See subsim1/Makefile: Makefile.defs' SFF assumes a two-levels-deep
# example; this directory is one level deeper.
SFF = ../../../../src

CFLAGS += -DFORSYDE_INTROSPECTION -DFORSYDE_WITH_MPI

CC = mpic++