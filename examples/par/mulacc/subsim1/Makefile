EXTRACFLAGS = 
EXTRA_LIBS =

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../../Makefile.defs

# Makefile.defs hardcodes SFF relative to a two-levels-deep example
# (examples/X/Y); this directory is one level deeper than that
# (examples/par/mulacc/subsim1), so its "../../../src" resolves to
# examples/src, which doesn't exist. Override with the correct depth.
SFF = ../../../../src

CFLAGS += -DFORSYDE_INTROSPECTION -DFORSYDE_WITH_MPI

CC = mpic++