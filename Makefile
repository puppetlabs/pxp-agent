##############################################################
#               CMake Project Wrapper Makefile               #
##############################################################

SHELL := /bin/sh
RM    := rm -rf

all: ./build/Makefile
	@ $(MAKE) -C build
	@ $(MAKE) test ARGS="-V"

./build/Makefile: Makefile CMakeLists.txt exe/CMakeLists.txt lib/CMakeLists.txt lib/tests/CMakeLists.txt
	@ mkdir build || true
	@ (cd build >/dev/null 2>&1 && cmake -DCMAKE_BUILD_TYPE=Debug -DEXTERNAL_CPP_PCP_CLIENT=OFF ..)

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
	@- $(RM) ./vendor/cpp-pcp-client/build
	@- $(RM) ./vendor/cpp-pcp-client/lib

ifeq (,$(findstring distclean,$(MAKECMDGOALS)))
    # delegate to ./build/Makefile if not distclean
    $(MAKECMDGOALS): ./build/Makefile
		@ $(MAKE) -C build $(MAKECMDGOALS)
endif
