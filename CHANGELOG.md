## 1.9.0

This is a feature release.

* [PCP-763](https://tickets.puppetlabs.com/browse/PCP-763) Cached tasks that have not been used in
`task-cache-dir-purge-ttl` - expressed as a time duration such as 30m, 4h, 1d - will be removed from disk. Running
them later will redownload from the `master-uri`. Default `task-cache-dir-purge-ttl` is `14d` (days).
* [PCP-791](https://tickets.puppetlabs.com/browse/PCP-791) pxp-agent will continue to function if task cache directory
is deleted. Specific task runs may fail, subsequent runs will recreate the cache directory and re-download task files.
* [PCP-511](https://tickets.puppetlabs.com/browse/PCP-511) Use hocon to load config. `.conf` files will be loaded
as HOCON, `.json` as JSON, and other file types will error.
* [PCP-792](https://tickets.puppetlabs.com/browse/PCP-792) When run as an unprivileged user, pxp-agent will now use
default user-directory paths, noted in
https://github.com/puppetlabs/puppet-specifications/blob/master/file_paths.md#puppet-agent-non-root.

## 1.8.1

This is a bug fix release.

* [PCP-826](https://tickets.puppetlabs.com/browse/PCP-826) Tasks that use the powershell input method (which is the
default for powershell scripts) require .NET Framework 3.5. PXP Agent will now give a clear error message when this
requirement is not satisfied.
* [PCP-825](https://tickets.puppetlabs.com/browse/PCP-825) PXP Agent will now correctly sync files using Windows line
endings (CRLF).
* [PCP-822](https://tickets.puppetlabs.com/browse/PCP-822) Tasks that didn't read standard input would occasionally return
an error when using the default input method. That seemed especially common on AIX platforms. The issue has been fixed.

## 1.8.0

This is a feature release.

* [PCP-780](https://tickets.puppetlabs.com/browse/PCP-780) Add `input_method: powershell` supporting
named params in powershell scripts.

## 1.7.0

This is a feature release.

* [PCP-786](https://tickets.puppetlabs.com/browse/PCP-786) Send error response to status if found
* [PCP-785](https://tickets.puppetlabs.com/browse/PCP-785) Report task download failure for 400+ HTTP statuses
* [#645](https://github.com/puppetlabs/pxp-agent/pull/645) Restrict permissions on spool files created by pxp-module-puppet
* [#644](https://github.com/puppetlabs/pxp-agent/pull/644) Prevent possible leak of `env_md_ctx_st` struct
* [PCP-782](https://tickets.puppetlabs.com/browse/PCP-782) Add port 8140 to `master-uris` if absent
* [#639](https://github.com/puppetlabs/pxp-agent/pull/639) Update task blocking response to return all results
* [PCP-783](https://tickets.puppetlabs.com/browse/PCP-783) Specify required sleep process more thoroughly
* [#641](https://github.com/puppetlabs/pxp-agent/pull/641) Fix permissions hacks
* [#635](https://github.com/puppetlabs/pxp-agent/pull/635) Support `input_method` on tasks
* [PCP-773](https://tickets.puppetlabs.com/browse/PCP-773) Implement client-side failover for task downloads
* [PCP-769](https://tickets.puppetlabs.com/browse/PCP-769) Download tasks from `master-uris`
* [#629](https://github.com/puppetlabs/pxp-agent/pull/629) Update appveyor config to use maintained packages
* [PCP-776](https://tickets.puppetlabs.com/browse/PCP-776) Detect disabled/running Puppet via lock files
* [PCP-766](https://tickets.puppetlabs.com/browse/PCP-766) Restrict permissions on spool files, logfile
* [#625](https://github.com/puppetlabs/pxp-agent/pull/625) Unblock acceptance tests on Japanese locale instance
* [PCP-633](https://tickets.puppetlabs.com/browse/PCP-633) Report running if action thread is still active
* [PCP-767](https://tickets.puppetlabs.com/browse/PCP-767) Introduce internal task module
* [#622](https://github.com/puppetlabs/pxp-agent/pull/622) Enable running acceptance from local package
* [PCP-768](https://tickets.puppetlabs.com/browse/PCP-768) Add `master-uris` config variable
* [PCP-775](https://tickets.puppetlabs.com/browse/PCP-775) Enable DEP support in Windows version of PCP/PXP binaries
* [PCP-758](https://tickets.puppetlabs.com/browse/PCP-758) Introduce puppet task support
* [#605](https://github.com/puppetlabs/pxp-agent/pull/605) Fix the test on win-10-ent-i386
* [#599](https://github.com/puppetlabs/pxp-agent/pull/599) Log program version and debug level at startup
* [#583](https://github.com/puppetlabs/pxp-agent/pull/583) Add testing around exec via Puppet

## 1.6.1

This is a maintenance and bug fix release.

* [PCP-755](https://tickets.puppetlabs.com/browse/PCP-755) Stop using versioned executables. Support
restarting the pxp-agent service when the binary path changes.
* [PCP-697](https://tickets.puppetlabs.com/browse/PCP-697) Ensure default log location exists on
service startup.
* [PCP-759](https://tickets.puppetlabs.com/browse/PCP-759) Enable testing against an EC2 master
* [PCP-750](https://tickets.puppetlabs.com/browse/PCP-750) Ensure failed connection attempts from test
code are closed before retrying.

Does not include fixes from 1.5.5.

## 1.6.0

This is a feature release.

* [PCP-603](https://tickets.puppetlabs.com/browse/PCP-603) Additional acceptance tests for pxp-module-puppet whitelist
* [PCP-699](https://tickets.puppetlabs.com/browse/PCP-699) pxp-module-puppet now accepts job-id as an argument and passes it to Puppet.
* [PCP-742](https://tickets.puppetlabs.com/browse/PCP-742) pxp-module-puppet will now return metrics about resource events when a run finishes.
* [PCP-748](https://tickets.puppetlabs.com/browse/PCP-748) Update pxp-agent component tests to allow SERVER_VERSION=latest
* [PCP-751](https://tickets.puppetlabs.com/browse/PCP-751) Remove kind from pxp-module-puppet

Does not include fixes from 1.5.5.

## 1.5.6

This is a maintenance release.

* [PCP-801](https://tickets.puppetlabs.com/browse/PCP-801) Use kill -15 rather than kill -s TERM for broader support

## 1.5.5

This is a maintenance release.

* [PCP-775](https://tickets.puppetlabs.com/browse/PCP-775) Enable DEP support via nxcompat and dynamicbase on Windows.

## 1.5.4

This is a maintenance release.

* [PCP-759](https://tickets.puppetlabs.com/browse/PCP-759) Enable acceptance pre-suite to install puppetserver on EC2
* [PCP-755](https://tickets.puppetlabs.com/browse/PCP-755) pxp-agent service now cleanly restarts after upgrade on SLES 11
* [PCP-750](https://tickets.puppetlabs.com/browse/PCP-750) Fix intermittent test failures due to inventory request timeouts
* [PCP-697](https://tickets.puppetlabs.com/browse/PCP-697) Ensure logdir exists before starting service

## 1.5.3

This is a maintenance release.

* [PCP-745](https://tickets.puppetlabs.com/browse/PCP-745) Populate missing manpage field in the SMF manifest

## 1.5.2

This is a maintenance release.

* [PCP-740](https://tickets.puppetlabs.com/browse/PCP-740) Fix umask test on AIX.

## 1.5.1

This is a maintenance release.

* [PCP-736](https://tickets.puppetlabs.com/browse/PCP-736) Run with inherited umask.

## 1.5.0

This is a feature release.

* [PCP-627](https://tickets.puppetlabs.com/browse/PCP-627) Now sends a provisional response when sent
a duplicate transaction id. Duplicates will also be detected across process sessions.
* [PCP-729](https://tickets.puppetlabs.com/browse/PCP-729) The default ping-interval is now 2 minutes.
This reduces the chance of disconnect/reconnect cycling when lots of agents connect to a broker at once.

## 1.4.1

This is a maintenance release.

* [#546](https://github.com/puppetlabs/pxp-agent/pull/546) Allow use of beaker 3.x in acceptance tests
* [PCP-704](https://tickets.puppetlabs.com/browse/PCP-704) Use fork() to prevent deadlocks

## 1.4.0

This is a feature release.

* [PCP-647](https://tickets.puppetlabs.com/browse/PCP-647) Add PCP version 2, an update that
changes to a simpler text-based protocol that only supports immediate delivery (no message
expiration) to a single target. It can be enabled by setting the "pcp-version" option to "2" and
connecting it to a version of pcp-broker supporting PCP v2.
* [PCP-469](https://tickets.puppetlabs.com/browse/PCP-469) Add a hidden option for configuring the
ping interval used to keep websocket connections alive.
* [PCP-624](https://tickets.puppetlabs.com/browse/PCP-624) Add a log file for messages delivered to
pxp-agent. It can be enabled using the `log-pcp-access` option.

## 1.3.2

This is a maintenance and bug fix release. It fixes several CI-related issues.

* [PCP-657](https://tickets.puppetlabs.com/browse/PCP-657) Previous fix for PCP-657 was incomplete.
Fixes detached processes on Solaris by invoking a detached process directly rather than using
`ctrun`. Using `ctrun` obfuscated the process ID of the task, causing its status to be reported
incorrectly.

## 1.3.1

This is a maintenance release.

* [PCP-276](https://tickets.puppetlabs.com/browse/PCP-276) Create new process group for external
modules on Windows
* [PCP-657](https://tickets.puppetlabs.com/browse/PCP-657) Allow unknown status in acceptance tests
* [PCP-625](https://tickets.puppetlabs.com/browse/PCP-625) Skip beaker time sync on Windows Server
2016
* [#514](https://github.com/puppetlabs/pxp-agent/pull/514) Change test to avoid depending on
delayed message delivery
* [#506](https://github.com/puppetlabs/pxp-agent/pull/506) Add beaker-abs compatible with beaker 2-3

## 1.3.0

This version introduces new features and maintains compatibility with the
[PXP v1.0 protocol](https://github.com/puppetlabs/pcp-specifications/tree/master/pxp/versions/1.0).

* [PCP-614](https://tickets.puppetlabs.com/browse/PCP-614) Update beaker version to 3.1.0
* [BKR-960](https://tickets.puppetlabs.com/browse/BKR-960) Skip timesync for ciscoxr-64a
* [PCP-605](https://tickets.puppetlabs.com/browse/PCP-605) Disable restart_host_run_puppet on Fedora
* [PCP-549](https://tickets.puppetlabs.com/browse/PCP-549) Wait for other Puppet runs to finish

## 1.2.3

This is a maintenance release. It fixes several CI-related issues.

## 1.2.2

This is a maintenance release.

* [BKR-958](https://tickets.puppetlabs.com/browse/BKR-958) Do not execute host restart test on
huaweios
* [PCP-601](https://tickets.puppetlabs.com/browse/PCP-601) Limit arguments to pxp-puppet flags to
needed characters
* [PCP-608](https://tickets.puppetlabs.com/browse/PCP-608) Bump version to 1.2.2
* [PCP-607](https://tickets.puppetlabs.com/browse/PCP-607) Delete accidentally merged acceptance
tests
* [PCP-601](https://tickets.puppetlabs.com/browse/PCP-601) Require command line flags be strings
and not include leading whitespace
* [PCP-576](https://tickets.puppetlabs.com/browse/PCP-576) Ensure cisconx tests can read lock file
* [PCP-572](https://tickets.puppetlabs.com/browse/PCP-572) Update how acceptance terminates sleep
processes
* [PCP-571](https://tickets.puppetlabs.com/browse/PCP-571) Fix tests failing with Puppet timing out
* [PCP-577](https://tickets.puppetlabs.com/browse/PCP-577) Don't run tests that restart the broker
on the master role
* [PCP-286](https://tickets.puppetlabs.com/browse/PCP-286) Fix occasional acceptance test failures
from pxp-agent service being left in the Paused state on Windows.
* [PCP-557](https://tickets.puppetlabs.com/browse/PCP-557) Ensure pcp-broker runs with internal
mirrors during testing
* [#475](https://github.com/puppetlabs/pxp-agent/pull/475) Fix Rakefile when specifying
BEAKER_HOSTS

## 1.2.1

This is a maintenance release.

* [PCP-545](https://tickets.puppetlabs.com/browse/PCP-545) Force Puppet output to UTF-8 when unknown
* [PCP-558](https://tickets.puppetlabs.com/browse/PCP-558) Acceptance: give UTF-8 test case its own
acceptance test
* [PCP-542](https://tickets.puppetlabs.com/browse/PCP-542) Allow parsing last_run_report with Puppet
objects
* [PCP-354](https://tickets.puppetlabs.com/browse/PCP-354) Acceptance: pcp-broker should use
internal mirror for deps
* [PCP-515](https://tickets.puppetlabs.com/browse/PCP-515) Acceptance: update ticket reference for
skipped test on Arista

## 1.2.0

This version introduces new features and maintains compatibility with the
[PXP v1.0 protocol](https://github.com/puppetlabs/pcp-specifications/tree/master/pxp/versions/1.0).

* [#464](https://github.com/puppetlabs/pxp-agent/pull/464) Acceptance: fix path for pxp-agent using
new Windows MSI layout
* [PCP-494](https://tickets.puppetlabs.com/browse/PCP-494) Fix external module and pxp-module-puppet
unicode handling
* [PCP-457](https://tickets.puppetlabs.com/browse/PCP-457) Acceptance: remove primary
dis-association check
* [PCP-457](https://tickets.puppetlabs.com/browse/PCP-457) Acceptance: automate pxp-agent broker
timeout failover test
* [PCP-514](https://tickets.puppetlabs.com/browse/PCP-514) Fix acceptance tests on Cisco Nexus
* [PCP-515](https://tickets.puppetlabs.com/browse/PCP-515) Acceptance: skip
restart_host_run_puppet.rb on Arista
* [PCP-508](https://tickets.puppetlabs.com/browse/PCP-508) Skip host restart test for CiscoNX
* [#455](https://github.com/puppetlabs/pxp-agent/pull/455) Fix gettext in Travis CI
* [PCP-493](https://tickets.puppetlabs.com/browse/PCP-493) Ensure stderr is returned in module
response
* [PCP-430](https://tickets.puppetlabs.com/browse/PCP-430) Automate pxp-agent broker failover
acceptance test
* [PCP-431](https://tickets.puppetlabs.com/browse/PCP-431) pxp agent acceptance should support a
failover pcp broker
* [PCP-479](https://tickets.puppetlabs.com/browse/PCP-479) Update pxp-agent to install batch
wrapper
* [PCP-478](https://tickets.puppetlabs.com/browse/PCP-478) Acceptance pre-suite should skip timesync
for Ubuntu
* [PCP-472](https://tickets.puppetlabs.com/browse/PCP-472) Ignore pxp-module-puppet `env` input arg
* [PCP-405](https://tickets.puppetlabs.com/browse/PCP-405) Make broker-ws-uris a config file only
option
* [#440](https://github.com/puppetlabs/pxp-agent/pull/440) Add libpxp-agent dependency on
horsewhisperer
* [PCP-463](https://tickets.puppetlabs.com/browse/PCP-463) Restore blocking monitor action
* [PCP-452](https://tickets.puppetlabs.com/browse/PCP-452) Add option to configure Association
timeout
* [PCP-454](https://tickets.puppetlabs.com/browse/PCP-454) Block execution thread once monitoring
starts
* [PCP-423](https://tickets.puppetlabs.com/browse/PCP-423) Add `allowed-keepalive-timeouts` option
* [PCP-416](https://tickets.puppetlabs.com/browse/PCP-416) Configure PCP message TTL; update HW
* [PCP-349](https://tickets.puppetlabs.com/browse/PCP-349) support running puppet as non-root
* [#428](https://github.com/puppetlabs/pxp-agent/pull/428) Remove unused local variables
* [#426](https://github.com/puppetlabs/pxp-agent/pull/426) Fix ActionRequest's log message format
* [PCP-383](https://tickets.puppetlabs.com/browse/PCP-383) Add broker failover
* [#425](https://github.com/puppetlabs/pxp-agent/pull/425) Remove unused include from pxp_schemas.cc

## 1.1.4

This is a maintenance release.

* [#456](https://github.com/puppetlabs/pxp-agent/pull/456) Fix version number
* [#449](https://github.com/puppetlabs/pxp-agent/pull/449) Switch to using project version
* [PCP-478](https://tickets.puppetlabs.com/browse/PCP-478) Acceptance pre-suite should skip timesync
* for Ubuntu
* [#443](https://github.com/puppetlabs/pxp-agent/pull/443) Test against Leatherman 0.7.4
* [PCP-453](https://tickets.puppetlabs.com/browse/PCP-453) Only kill pxp-agent if it's running in
* logrotate
* [PCP-428](https://tickets.puppetlabs.com/browse/PCP-428) fix restart_host test on windows agents
* [PCP-428](https://tickets.puppetlabs.com/browse/PCP-428) define missing variable in
* restart_host_run_puppet test
* [PCP-428](https://tickets.puppetlabs.com/browse/PCP-428) restart_host_run_puppet.rb transient fails
* on ubuntu1404
* [PCP-228](https://tickets.puppetlabs.com/browse/PCP-228) Acceptance - run puppet and expect failure

## 1.1.3

This is a maintenance release.

* [#422](https://github.com/puppetlabs/pxp-agent/pull/422) Restore pluralized messages
* [PCP-306](https://tickets.puppetlabs.com/browse/PCP-306) Use last_run_report mtime to check for
updates
* [#419](https://github.com/puppetlabs/pxp-agent/pull/419) C++ Style Cleanup
* [#418](https://github.com/puppetlabs/pxp-agent/pull/418) Cleanup HorseWhisperer enum use
* [#421](https://github.com/puppetlabs/pxp-agent/pull/421) Remove old Catch installation
* [#417](https://github.com/puppetlabs/pxp-agent/pull/417) Add cpplint to Travis's target matrix
* [#414](https://github.com/puppetlabs/pxp-agent/pull/414) Fix up AppVeyor
* [PCP-381](https://tickets.puppetlabs.com/browse/PCP-381) Fix usage of env values in acceptance
Rakefile
* [PCP-371](https://tickets.puppetlabs.com/browse/PCP-371) Spike i18n support
* [PCP-196](https://tickets.puppetlabs.com/browse/PCP-196) Fix log rotation on Ubuntu 14.04 LTS
* [#407](https://github.com/puppetlabs/pxp-agent/pull/407) Download CMake from our S3 bucket
* [#406](https://github.com/puppetlabs/pxp-agent/pull/406) Enable builds with static cpp-pcp-client
* [PCP-230](https://tickets.puppetlabs.com/browse/PCP-230) Acceptance - restart pxp-agent during
Puppet run
* [#404](https://github.com/puppetlabs/pxp-agent/pull/404) Look for .dylib in
Findcpp-pcp-client.cmake
* [#403](https://github.com/puppetlabs/pxp-agent/pull/403) Add CONTRIBUTING and Maintenance doc
sections
* [PCP-379](https://tickets.puppetlabs.com/browse/PCP-379) Store expanded --spool-dir in HW
singleton
* [#400](https://github.com/puppetlabs/pxp-agent/pull/400) Add README section on running against a
test broker
* [#396](https://github.com/puppetlabs/pxp-agent/pull/396) Make tests compatible with leatherman 0.4
and 0.5
* [PCP-374](https://tickets.puppetlabs.com/browse/PCP-374) Update Catch configuration
* [PCP-370](https://tickets.puppetlabs.com/browse/PCP-370) Remove sets of SSL certificates
* [PCP-345](https://tickets.puppetlabs.com/browse/PCP-345) Create spool dir if needed
* [#388](https://github.com/puppetlabs/pxp-agent/pull/388) Refactor Configuration
* [PCP-284](https://tickets.puppetlabs.com/browse/PCP-284) In modules interface, specify "metadata"
arg
* [#374](https://github.com/puppetlabs/pxp-agent/pull/374) Add name prefix for ThreadContainer unit
tests
* [PCP-344](https://tickets.puppetlabs.com/browse/PCP-344) Rely on cpp-pcp-client's ttl_expired
callback
* [#373](https://github.com/puppetlabs/pxp-agent/pull/373) Remove move() when returning unique_ptr
* [PCP-338](https://tickets.puppetlabs.com/browse/PCP-338) Handle Associate Session errors
* [PCP-340](https://tickets.puppetlabs.com/browse/PCP-340) Wait for the module completion in
component tests
* [PCP-308](https://tickets.puppetlabs.com/browse/PCP-308) run modules in own contracts on Solaris
* [#365](https://github.com/puppetlabs/pxp-agent/pull/365) Consider output processing delay in
component test

## 1.1.2

This is a maintenance release.

* [#397](https://github.com/puppetlabs/pxp-agent/pull/397) Fixes for Leatherman 0.6.0 on stable
* [PCP-376](https://tickets.puppetlabs.com/browse/PCP-376) Acceptance - do not attempt timesync on OSX
* [#392](https://github.com/puppetlabs/pxp-agent/pull/397) Update ruby-pcp-client to 0.4.0 in testing
* [PCP-375](https://tickets.puppetlabs.com/browse/PCP-375) Remove usage of test fixture SSL files from
acceptance tests
* [PCP-355](https://tickets.puppetlabs.com/browse/PCP-355) Show logs on acceptance test failures
* [PCP-307](https://tickets.puppetlabs.com/browse/PCP-307) Acceptance tests should expect
'maintenance' service state on Solaris
* [PCP-363](https://tickets.puppetlabs.com/browse/PCP-363) Skip test that restarts host for AIX
agents
* [PCP-329](https://tickets.puppetlabs.com/browse/PCP-329) Update ruby-pcp-client to pick up
EventMachine improvements
* [PCP-364](https://tickets.puppetlabs.com/browse/PCP-364) Acceptance - sync system time on hosts
* [PCP-359](https://tickets.puppetlabs.com/browse/PCP-359) Fix confusion between EL and CentOS in
acceptance host generation
* [PCP-305](https://tickets.puppetlabs.com/browse/PCP-305) Acceptance tests on OSX should not expect
a stopped service
* [PCP-360](https://tickets.puppetlabs.com/browse/PCP-360) Acceptance: puppet-agent should use
beaker hostname as certname
* [#377](https://github.com/puppetlabs/pxp-agent/pull/377) Acceptance - remove static host config
for Cumulus
* [PCP-232](https://tickets.puppetlabs.com/browse/PCP-232) Acceptance - ensure pxp-agent is usable
after restarting host
* [PCP-225](https://tickets.puppetlabs.com/browse/PCP-225) Acceptance test for attempting puppet run
on a disabled agent

## 1.1.1

This version integrates the changes made for 1.0.3.

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

This is a security release.

* [PCP-326](https://tickets.puppetlabs.com/browse/PCP-326) add acceptance tests
 for SSL validation
* [PCP-328](https://tickets.puppetlabs.com/browse/PCP-328) update acceptance
 tests
* [PCP-321](https://tickets.puppetlabs.com/browse/PCP-321) pxp-module-puppet now
 implements a white list for puppet-agent's flags

## 1.0.2

This is a maintenance release.

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

This is a maintenance release.

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

This is the first release.
