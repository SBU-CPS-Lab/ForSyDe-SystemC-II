EXTRACFLAGS = 
EXTRA_LIBS =

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../../Makefile.defs

CFLAGS += -DFORSYDE_INTROSPECTION -DFORSYDE_WITH_MPI

CC = mpic++