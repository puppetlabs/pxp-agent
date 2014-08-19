##############################################################
#               CMake Project Wrapper Makefile               #
##############################################################

SHELL := /bin/sh
RM    := rm -rf

all: ./build/Makefile
	@ $(MAKE) -C build

build:
	@ mkdir build

./build/Makefile: build
	@ (cd build >/dev/null 2>&1 && cmake ..)

distclean:
	@- (cd build >/dev/null 2>&1 && cmake .. >/dev/null 2>&1)
	@- $(MAKE) --silent -C build clean || true
	@- $(RM) ./build/Makefile
	@- $(RM) ./build/src
	@- $(RM) ./build/test
	@- $(RM) ./build/CMake*
	@- $(RM) ./build/cmake.*
	@- $(RM) ./build/*.cmake
	@- $(RM) ./build/*.txt
	@- $(RM) ./docs/*.html
	@- $(RM) ./docs/*.css
	@- $(RM) ./docs/*.png
	@- $(RM) ./docs/*.jpg
	@- $(RM) ./docs/*.gif
	@- $(RM) ./docs/*.tiff
	@- $(RM) ./docs/*.php
	@- $(RM) ./docs/search
	@- $(RM) ./docs/installdox

ifeq (,$(findstring distclean,$(MAKECMDGOALS)))
    # delegate to ./build/Makefile if not distclean
    $(MAKECMDGOALS): ./build/Makefile
		@ $(MAKE) -C build $(MAKECMDGOALS)
endif
