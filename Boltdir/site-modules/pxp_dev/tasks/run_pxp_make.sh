#/usr/bin/env sh

pushd $(find /var/tmp/ -maxdepth 2 -name pxp-agent) >/dev/null 1>&2
if [[ $PT_run_cmake == "true" ]]; then
  export PATH="/opt/puppetlabs/puppet/bin:/opt/pl-build-tools/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/root/bin" && \
  /opt/pl-build-tools/bin/cmake -DCMAKE_TOOLCHAIN_FILE=/opt/pl-build-tools/pl-build-toolchain.cmake \
                                -DLEATHERMAN_GETTEXT=ON \
                                -DCMAKE_VERBOSE_MAKEFILE=ON \
                                -DCMAKE_PREFIX_PATH=/opt/puppetlabs/puppet \
                                -DCMAKE_INSTALL_RPATH=/opt/puppetlabs/puppet/lib \
                                -DCMAKE_SYSTEM_PREFIX_PATH=/opt/puppetlabs/puppet \
                                -DMODULES_INSTALL_PATH=/opt/puppetlabs/pxp-agent/modules \
                                -DCMAKE_INSTALL_PREFIX=/opt/puppetlabs/puppet
fi
make -j8 install
popd >/dev/null 1>&2
