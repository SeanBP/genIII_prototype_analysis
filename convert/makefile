SHELL       := /bin/bash
CXXFLAGS    := -std=c++11
VPATH	    := include

root_libs    = `root-config --libs --glibs --cflags`

# include file
calo	    := calo.h
utilities   := utilities.h
convert	    := convert.h

cali_include = calo.h utilities.h convert.h 

convert: convert.C 
	g++ $(CXXFLAGS) -o $@ $^ $(root_libs)

all: convert
# vim: set shiftwidth=4 softtabstop=4 tabstop=8: #
