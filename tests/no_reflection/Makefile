EXTRACFLAGS =
EXTRA_LIBS =

MODULE = run
SRCS = $(wildcard *.cpp)

OBJS = $(SRCS:.cpp=.o)

include ../../examples/Makefile.defs

# Makefile.defs hardcodes SFF relative to a two-levels-deep example
# (examples/X/Y). This directory is tests/no_reflection, two levels deep from
# the repo root rather than three, so override with the correct depth.
SFF = ../../src

# The point of this directory: the harness's "on" row is the opt-out
# build. Its "off" row overrides CFLAGS wholesale and so drops this,
# giving an ordinary reflection-on build.
CFLAGS += -DFORSYDE_NO_REFLECTION

