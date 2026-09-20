EXTRACFLAGS =
EXTRA_LIBS =

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../examples/Makefile.defs

# Makefile.defs hardcodes SFF relative to a two-levels-deep example
# (examples/X/Y). This directory is tests/sdf3, two levels deep from the
# repo root rather than three, so override with the correct depth.
SFF = ../../src

CFLAGS += -DFORSYDE_INTROSPECTION

