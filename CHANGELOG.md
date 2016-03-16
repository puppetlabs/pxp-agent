## 1.1.2

## 1.1.1

## 1.1.0

This version introduces new features and maintains compatibility with the
[PXP v1.0 protocol](https://github.com/puppetlabs/pcp-specifications/tree/master/pxp/versions/1.0).

* [PCP-315](https://tickets.puppetlabs.com/browse/PCP-315) Fix a bug that consisted in pxp-module
puppet returning bad output on stdout in case of invalid input
* [PCP-275](https://tickets.puppetlabs.com/browse/PCP-275) Add functionality to periodically purge
the spool directory
* [#347](https://github.com/puppetlabs/pxp-agent/pull/347) Acceptance tests improvements
* [#334](https://github.com/puppetlabs/pxp-agent/pull/334) Fix event machine usage for PCP client in
acceptance tests
* [PCP-208](https://tickets.puppetlabs.com/browse/PCP-208) Process the output of actions when
retrieved from file and implement new data structures for action processing; this allows to retrieve
and validate the output of actions that completed after pxp-agent stopped
* [PCP-288](https://tickets.puppetlabs.com/browse/PCP-288) Set systemd's KillMode to not kill child
processes when pxp-agent stops
* [PCP-297](https://tickets.puppetlabs.com/browse/PCP-297) Ensure that the PID directory exists if
the default path is configured (bug on Solaris)
* [#339](https://github.com/puppetlabs/pxp-agent/pull/339) Use ruby-pcp-client 0.2.0 for acceptance
tests
* [PCP-227](https://tickets.puppetlabs.com/browse/PCP-227) Improve logic that processes the output
of pxp-module-puppet in acceptance tests
* [#338](https://github.com/puppetlabs/pxp-agent/pull/338) Improve client logic in acceptance tests
* [#337](https://github.com/puppetlabs/pxp-agent/pull/337) Minor quoting fix in acceptance test
* [#336](https://github.com/puppetlabs/pxp-agent/pull/336) Acceptance tests improvements
* [#334](https://github.com/puppetlabs/pxp-agent/pull/334) Fix timing issues in acceptance tests
* [#332](https://github.com/puppetlabs/pxp-agent/pull/332) Fix timing issues in acceptance tests
* [#328](https://github.com/puppetlabs/pxp-agent/pull/328) Fix dependencies on AIX
* [PCP-202](https://tickets.puppetlabs.com/browse/PCP-202) Update internal logic to consider as
successful an action run with valid output
* [#324](https://github.com/puppetlabs/pxp-agent/pull/324) Add dependency on librt
* [#321](https://github.com/puppetlabs/pxp-agent/pull/321) Remove outdated Makefile
* [#322](https://github.com/puppetlabs/pxp-agent/pull/322) ThreadContainer now caches the
transaction IDs in a map
* [PCP-255](https://tickets.puppetlabs.com/browse/PCP-255) Improve logging messages
* [PCP-239](https://tickets.puppetlabs.com/browse/PCP-239) Component tests for the new external
modules' interface
* [PCP-251](https://tickets.puppetlabs.com/browse/PCP-251) pxp-module-puppet now supports the new
external modules' interface and writes its output on file
* [PCP-198](https://tickets.puppetlabs.com/browse/PCP-198) Use Beaker methods in acceptance tests
when installing puppet-agent
* [PCP-235](https://tickets.puppetlabs.com/browse/PCP-235) Use Beaker methods in acceptance tests
when setting up pcp-broker
* [PCP-207](https://tickets.puppetlabs.com/browse/PCP-207) Internal changes for supporting the new
external modules' interface
* [PCP-209](https://tickets.puppetlabs.com/browse/PCP-209) Remove git submodules for leatherman and
cpp-pcp-client
* [PCP-240](https://tickets.puppetlabs.com/browse/PCP-240) Formalize and extend the external
modules' documentation
* [PCP-188](https://tickets.puppetlabs.com/browse/PCP-188) New acceptance tests
* [#293](https://github.com/puppetlabs/pxp-agent/pull/293) Improve external modules' documentation

## 1.0.3

This is a security release

* [PCP-326](https://tickets.puppetlabs.com/browse/PCP-326) add acceptance tests
 for SSL validation
* [PCP-328](https://tickets.puppetlabs.com/browse/PCP-328) update acceptance
 tests
* [PCP-321](https://tickets.puppetlabs.com/browse/PCP-321) pxp-module-puppet now
 implements a white list for puppet-agent's flags

## 1.0.2

This is a maintenance release

* [PCP-238](https://tickets.puppetlabs.com/browse/PCP-238) Fixed a bug that prevented pxp-agent from
loading modules' configuration files named with ".conf" suffix
* [PCP-234](https://tickets.puppetlabs.com/browse/PCP-234) Fixed a bug that prevented pxp-agent from expanding file paths passed via --module-dir and --modules-config-dir options
* [PCP-223](https://tickets.puppetlabs.com/browse/PCP-223) Acceptance tests improvements (use helper lib files and run test steps on all agent hosts; remove static config files)
* [PCP-233](https://tickets.puppetlabs.com/browse/PCP-233) Add Arista support to acceptance tests
* [PCP-234](https://tickets.puppetlabs.com/browse/PCP-245) Add new set of SSL certificates for testing
* [QENG-3181](https://tickets.puppetlabs.com/browse/QENG-3181) Use beaker-hostgenerator instead of sqa-utils
* [#280](https://github.com/puppetlabs/pxp-agent/pull/280) Improve acceptance test based on log message parsing
* [#278](https://github.com/puppetlabs/pxp-agent/pull/278) Fixed brittle acceptance test regex
* [PCP-196](https://tickets.puppetlabs.com/browse/PCP-196) Fixed logrotate functionality for debian systems

## 1.0.1

This is a maintenance release

* [PCP-172](https://tickets.puppetlabs.com/browse/PCP-172) Fixed a race condition
  when checking the status of an action after the action finished but before
  the metadata file has been updated.
* [#244](https://github.com/puppetlabs/pxp-agent/pull/244) Add INFO level log message
  when sending a blocking response.
* [#247](https://github.com/puppetlabs/pxp-agent/pull/247)
  [#243](https://github.com/puppetlabs/pxp-agent/pull/243)
  [PCP-143](https://tickets.puppetlabs.com/browse/PCP-172) Acceptance test
  improvements

## 1.0.0

This is the first release
