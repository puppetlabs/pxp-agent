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
