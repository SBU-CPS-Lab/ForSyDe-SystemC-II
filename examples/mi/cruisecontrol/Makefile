# FORSYDE_WITH_FMI's XML parser (D15) uses libxml2's streaming reader.
EXTRACFLAGS = $(shell pkg-config --cflags libxml-2.0)
EXTRA_LIBS = -lmigdb $(shell pkg-config --libs libxml-2.0)

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../Makefile.defs

CFLAGS += -DFORSYDE_INTROSPECTION -DFORSYDE_WITH_GDB -DFORSYDE_WITH_FMI

