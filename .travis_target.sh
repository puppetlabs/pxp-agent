#!/bin/bash
set -ev

if [ ${TRAVIS_TARGET} == MODULES ]; then
  cd modules
  bundle install
  bundle exec rspec spec
  exit
fi

# Set compiler to GCC 4.8 here, as Travis overrides the global variables.
export CC=gcc-4.8 CXX=g++-4.8

if [ ${TRAVIS_TARGET} == CPPCHECK ]; then
  # grab a pre-built cppcheck from s3
  wget https://s3.amazonaws.com/kylo-pl-bucket/pcre-8.36_install.tar.bz2
  tar xjvf pcre-8.36_install.tar.bz2 --strip 1 -C $USERDIR
  wget https://s3.amazonaws.com/kylo-pl-bucket/cppcheck-1.69_install.tar.bz2
  tar xjvf cppcheck-1.69_install.tar.bz2 --strip 1 -C $USERDIR
elif [ ${TRAVIS_TARGET} == DEBUG ]; then
  # Install coveralls.io update utility
  pip install --user cpp-coveralls
fi

git clone https://github.com/puppetlabs/cpp-pcp-client
cd cpp-pcp-client
git checkout 1.0.1
git submodule update --init --recursive
cmake -DCMAKE_INSTALL_PREFIX=$USERDIR .
make install -j2
cd ..

# Generate build files
if [ ${TRAVIS_TARGET} == DEBUG ]; then
  TARGET_OPTS="-DCMAKE_BUILD_TYPE=Debug -DCOVERALLS=ON"
fi
cmake $TARGET_OPTS -DCMAKE_PREFIX_PATH=$USERDIR -DCMAKE_INSTALL_PREFIX=$USERDIR .

if [ ${TRAVIS_TARGET} == CPPLINT ]; then
  make cpplint
elif [ ${TRAVIS_TARGET} == CPPCHECK ]; then
  make cppcheck
else
  make -j2
  make test ARGS=-V

  # Make sure installation succeeds
  make install
fi

