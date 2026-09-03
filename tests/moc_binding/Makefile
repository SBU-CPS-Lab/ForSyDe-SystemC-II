EXTRACFLAGS =
EXTRA_LIBS =

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../examples/Makefile.defs

# Makefile.defs hardcodes SFF relative to a two-levels-deep example
# (examples/X/Y). This directory is tests/moc_binding, two levels deep from
# the repo root rather than three, so override with the correct depth.
SFF = ../../src

CFLAGS += -DFORSYDE_INTROSPECTION

# This test is the one place the D13 binding assertions are switched on.
CFLAGS += -DFORSYDE_STRICT_MOC
