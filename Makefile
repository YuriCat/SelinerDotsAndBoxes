# 
# 1. General Compiler Settings
#
CXX       = g++
CXXFLAGS  = -std=c++14 -Wall -Wextra -Wcast-qual -Wno-unused-function -Wno-sign-compare -Wno-unused-value -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -fno-rtti\
            -pedantic -Wno-long-long -msse4.2 -mbmi -mbmi2 -mavx2 -D__STDC_CONSTANT_MACROS -fopenmp
INCLUDES  =
LIBRARIES = -lpthread

#
# 2. Target Specific Settings
#
ifeq ($(TARGET),match)
	CXXFLAGS += -Ofast -DNDEBUG -DMINIMUM -DMATCH
        output_dir := out/match/
endif
ifeq ($(TARGET),release)
	CXXFLAGS += -Ofast -DNDEBUG -DMINIMUM
        output_dir := out/release/
endif
ifeq ($(TARGET),debug)
	CXXFLAGS += -O0 -g -ggdb -DDEBUG -DBROADCAST -D_GLIBCXX_DEBUG
        output_dir := out/debug/
endif
ifeq ($(TARGET),default)
	CXXFLAGS += -O2 -g -ggdb
        output_dir := out/default/
endif

#
# 2. Default Settings (applied if there is no target-specific settings)
#
sources      ?= $(shell ls -R src/*.cc)
sources_dir  ?= src/
objects      ?=
directories  ?= $(output_dir)

#
# 4. Public Targets
#
default release debug:
	$(MAKE) TARGET=$@ preparation dab_test vs

match:
	$(MAKE) TARGET=$@ preparation

clean:
	rm -rf out/*

#
# 5. Private Targets
#
preparation $(directories):
	mkdir -p $(directories)

dab_test :
	$(CXX) $(CXXFLAGS) -o $(output_dir)dab_test $(sources_dir)dab_test.cc $(LIBRARIES)

vs :
	$(CXX) $(CXXFLAGS) -o $(output_dir)vs $(sources_dir)vs.cc $(LIBRARIES)

-include $(dependencies)