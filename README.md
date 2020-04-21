# PXP Agent

This is the agent for the PCP Execution Protocol [(PXP)][pxp_specs_root], based
on the the Puppet Communications Protocol [(PCP)][pcp_specs_root]. It enables
the execution of [actions][pxp_specs_actions] on remote nodes.

The PCP interface is provided by [cpp-pcp-client][cpp-pcp-client] which, in
turn, relies on [websocket++][websocketpp].

pxp-agent needs to be connected to a [PCP broker][pcp-broker] to operate; please
refer to the documentation below for how to do that.

Dependencies
------------

 - a C++11 compiler (clang/gcc 4.7)
 - gnumake
 - CMake
 - Boost
 - OpenSSL
 - ruby (2.0 and newer)
 - [leatherman][leatherman] (0.5.1 or newer)
 - [cpp-pcp-client][cpp-pcp-client] (master)

Initial Setup
-------------

#### Setup on Fedora 23

The following will install all required tools and libraries:

    yum install boost-devel openssl-devel gcc-c++ make wget tar cmake

#### Setup on Mac OSX El Capitan (homebrew)

This assumes Clang is installed and the system OpenSSL libraries will be used.

The following will install all required libraries:

    brew install cmake boost

#### Setup on Ubuntu 15.10 (Trusty)

The following will install most required tools and libraries:

    apt-get install build-essential libboost-all-dev libssl-dev wget tar cmake

#### Setup on Windows

[MinGW-w64][MinGW-w64] is used for full C++11 support, and
[Chocolatey][Chocolatey] can be used to install. You should have at least 2GB of
memory for compilation.

* install [CMake][CMake-choco] & [7zip][7zip-choco]

        choco install cmake 7zip.commandline

* install [MinGW-w64][MinGW-w64-choco]

        choco install mingw --params "/threads:win32"

For the remaining tasks, build commands can be executed in the shell from:

        Start > MinGW-w64 project > Run Terminal

* select an install location for dependencies, such as C:\\tools or cmake\\release\\ext; we'll refer to it as $install

* build [Boost][Boost-download]

        .\bootstrap mingw
        .\b2 toolset=gcc --build-type=minimal install --prefix=$install --with-program_options --with-system --with-filesystem --with-date_time --with-thread --with-regex --with-log --with-locale --with-chrono boost.locale.iconv=off

In Powershell:

    choco install cmake 7zip.commandline -y
    choco install mingw --params "/threads:win32" -y
    $env:PATH = "C:\tools\mingw64\bin;$env:PATH"
    $install = "C:\tools"

    (New-Object Net.WebClient).DownloadFile("https://downloads.sourceforge.net/boost/boost_1_54_0.7z", "$pwd/boost_1_54_0.7z")
    7za x boost_1_54_0.7z
    pushd boost_1_54_0
    .\bootstrap mingw
    .\b2 toolset=gcc --build-type=minimal install --prefix=$install --with-program_options --with-system --with-filesystem --with-date_time --with-thread --with-regex --with-log --with-locale --with-chrono boost.locale.iconv=off
    popd

Build
-----

* build & install [leatherman][leatherman]

* build & install [cpp-pcp-client][cpp-pcp-client]

* build the pxp-agent

  Thanks to the CMake, the project can be built out-of-source tree, which allows for
  multiple independent builds.
  Aside from the standard CMake switches the build supports the following option:

   * **DEV_LOG_COLOR** enables colorization for logging (development setting)
     (default _OFF_)

  example release build:

      mkdir release
      cd release
      cmake ..
      make

  example debug/test build:

      mkdir debug
      cd debug
      cmake -DCMAKE_BUILD_TYPE=Debug -DDEV_LOG_COLOR=ON ..
      make

  NOTE: If the versions of OpenSSL and libcurl conflict with each other, curl may fail
  to load SSL files. On macOS this is common when using Homebrew. Invoke cmake with the
  following commands to use the Homebrew versions of OpenSSL and libcurl:
    cmake -DCMAKE_PREFIX_PATH="/usr/local/opt/openssl;/usr/local/opt/curl" ..

Usage
-----

### Starting the agent

pxp-agent should be configured in your system to be executed automatically as a
service. In case you need to run it manually, you can invoke directly its
executable file; after building from source, it is located in the `./build/bin`
directory; in case an installer was used, the default locations are:

 - \*nix: */opt/puppetlabs/puppet/bin/pxp-agent*
 - Windows: *C:\Program Files\Puppet Labs\Puppet\pxp-agent\bin\pxp-agent.exe*

Configuration options can be passed as command line arguments or by using a
configuration file (see below).

The agent will execute as a background process by default; in that case,
it prevents multiple instances running at the same time. Please refer to the
following sections for platform-specific behavior.

#### *nix

In case `--foreground` is unflagged, pxp-agent will start as a daemon and its
PID will be stored in */var/run/puppetlabs/pxp-agent.pid*, or in another
location if specified by `--pidfile`. pxp-agent will rely on such file to
prevent multiple daemon instances from executing at once. The PID file will be
removed if the daemon is stopped by using one of SIGINT, SIGTERM, or SIGQUIT
signals.

#### Windows

pxp-agent relies on [nssm][nssm] to execute as a service. In case `--foreground`
is unflagged, a mutex-based mechanism will prevent multiple instances of
pxp-agent. Note that no PID file will be created.

#### Exit code

In POSIX, when the daemon is successfully instantiated, the parent process
returns 0. In case of a daemonization failure, it returns 4.

In case of it fails to parse the command line options or the configuration file,
pxp-agent returns 2.

In case of invalid configuration, say an option is set to an invalid value, it
returns 3.

For all other failures it returns 1.

### Modules

[Actions][pxp_specs_actions] are grouped in modules, by which they can be loaded
and configured within pxp-agent. An example of module is given by the
[Puppet module][pxp-module-puppet_script]; a trivial one is the
[reverse module][pxp-module-puppet_docs] that is used for testing.

#### Modules interface

A module is a file that provides an interface to retrieve information about
its actions (we call such information metadata - it's a set of JSON schemas) and
to execute actions.

The metadata is used by pxp-agent to acquire knowledge about the module's
actions and validate its configuration. For each action, the metadata specifies
the format of the input arguments and the output results. Please refer to
[this document][modules_docs] for more details on requirements for modules.

To run a given action, pxp-agent invokes the module with the action
name. The input specified in the [PXP request][pxp_specs_request_response] and
other parameters will be then passed to the module via stdin.

pxp-agent invokes modules directly, as executables. For determining the paths of
the executables, pxp-agent will inspect the `--modules-dir` directory and
look for:

 - **POSIX**: files without any suffix;
 - **Windows**: files with ".bat" extension.

Note that the [transaction status module][pxp_specs_transaction_status] is
implemented natively; there is no module file for it. Also, as a side note,
`status query` requests must be of [blocking][pxp_specs_request_response].

#### Modules configuration

Modules can be configured by placing a configuration file in the
`--modules-config-dir` named like `<module_name>.conf`. The content of a
configuration file must be in JSON format and conform with the `configuration`
schema provided by the module's metadata, otherwise the module will not be
loaded.

### Configuring the agent

The PXP agent is configured with a config file. The values in the config file
can be overridden by supplying arguments on the command line.

The agent will look for the default config file in:
 - \*nix: */etc/puppetlabs/pxp-agent/pxp-agent.conf*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\etc\pxp-agent.conf*

A different config file can be specified by passing the `--config-file` option.

The config files use the JSON format. Options must be specified as entries of a
single JSON object. Example:

```
{
    "broker-ws-uris" : ["wss://pcp_broker_cn:8142/pcp/", "wss://pcp_alt_broker_cn:8142/pcp/"],
    "ssl-key" : "/etc/puppetlabs/puppet/ssl/private_keys/myhost.net.pem",
    "ssl-ca-cert" : "/etc/puppetlabs/puppet/ssl/certs/ca.pem",
    "ssl-cert" : "/etc/puppetlabs/puppet/ssl/certs/myhost.net.pem"
}
```

Note that you have to specify the WebSocket secure URI of the
[PCP broker][pcp-broker] and the certificate of the CA that is used by it, in
order to establish the WebSocket connection on top of which the PCP
communication will take place (PCP uses secure WebSocket). Also, the hostname
used in the WebSocket URI must match the SSL identity used by the broker.

### Testing against a test broker

The simple instructions for setting up a test [PCP broker][pcp-broker] use pre-
generated certs present in that repo. To connect to this test broker, you'll first
need to add a hosts config that redirects `broker.example.com` to the broker host
so that server certificate verification succeeds. Then point pxp-agent at `ca`
and a client certificate present in [PCP broker][pcp-broker]. Example:

```
puppet resource host broker.example.com ip=<host ip>
pxp-agent --broker-ws-uri wss://broker.example.com:8142/pcp \
          --ssl-ca-cert $pcp_broker/test-resources/ssl/certs/ca.pem \
          --ssl-cert $pcp_broker/test-resources/ssl/certs/client01.example.com.pem \
          --ssl-key $pcp_broker/test-resources/ssl/private_keys/client01.example.com.pem \
          --spool-dir dev-resources/spool \
          --modules-dir modules \
          --logfile - --loglevel debug --foreground
```

#### Logging

By default, log messages will be written to:
 - \*nix: */var/log/puppetlabs/pxp-agent/pxp-agent.log*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\var\log\pxp-agent.log*.

You can specify a different file with the `--logfile` option.

When running in foreground mode, it is possible to display log messages on
console by using an hyphen instead of a file path: `--logfile -`.

The default log level is `info`. You can specify a different log level by
using the `--loglevel` option with one of the following strings: `none`,
`trace`, `debug`, `info`, `warning`, `error`, `fatal`.

#### PCP Access Logging

The `pcp_access` logger provides information about incoming PCP messages.
You can enable it by setting the `log-pcp-access` flag.

The default location of the `pcp_access` log file is:
 - \*nix: */var/log/puppetlabs/pxp-agent/pxp-access.log*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\var\log\pxp-access.log*.
You can specify a different file with the `pcp-access-logfile` option.

Each pcp_access entry is composed of 6 fields:

```
[<date time>] <access outcome> <broker: WS URI> <sender: PCP URI> <PCP messagetype> <PCP message id>
```

For example:

```
[2016-08-24 13:56:10.737760] AUTHORIZATION_SUCCESS wss://localhost:8142/pcp pcp:///server http://puppetlabs.com/associate_response 9766ba60-a51f-4910-9921-c76990aa9b38
[2016-08-24 14:07:51.859244] AUTHORIZATION_SUCCESS wss://localhost:8142/pcp/vNext pcp://peg.example.com/peg-controller http://puppetlabs.com/rpc_blocking_request a06e371a-08a2-47f0-913c-15780d668e2f
```

The second entry gives the outcome of the message validation; possible values
are:

| validation outcome | description
|--------------------|------------
| DESERIALIZATION_ERROR | invalid PCP message that can't be deserialized
| AUTHORIZATION_SUCCESS | the message will be processed by pxp-agent

#### List of all configuration options

The PXP agent has the following configuration options

**config-file (optional)**

Specify which config file to use.

**broker-ws-uri (required to connect)**

The WebSocket URI of the PXP broker you wish to connect the agent to, in the
`wss://<broker identity>:8142/pcp/` format; example:
*wss://pcp_broker_cn:8142/pcp/*

**broker-ws-uris (alternative to broker-ws-uri)**

A config file only alternative to broker-ws-uri, that takes an array
specifying multiple brokers the agent can connect to. When multiple brokers
are specified, it will use them in a failover capacity, where if it's unable
to connect to one it will try the next in the list, and repeat until a
successful connection is made. In the event of a disconnect, the agent will
retry that connection before trying a new broker.

**broker-ws-proxy (optional)**

Proxy URI for websocket connection with PCP broker. 

**master-uris (optional)**

An array of HTTPS URIs of servers hosting task files for download. When multiple
are specified, it will use them in a failover capacity, where if it's unable
to connect to one it will try the next in the list until all have been tried.
If all are unavailable, task download will fail.

**master-proxy (optional)**

Proxy URI for downloading tasks from master. 

**pcp-version (optional)**

Specifies whether to use PCP version 1 or 2. Only accepts '1' or '2'. Defaults to '1'.

**connection-timeout (optional)**

Maximum amount of time that may elapse when trying to establish a connection to
the broker in seconds. Defaults to 5 seconds.

**ssl-ca-cert (required to connect)**

The location of your SSL Certificate Authority certificate, example
/etc/puppet/ssl/ca/ca_crt.pem

**ssl-cert (required to connect)**

The location of the pxp-agent SSL certificate, example /etc/puppet/ssl/certs/bob_crt.pem

**ssl-key (required to connect)**

The location of the pxp-agent's SSL private key, example /etc/puppet/ssl/certs/bob_key.pem

**ssl-crl (optional)**

The location of the pxp-agent's SSL certificate revocation list, example /etc/puppetlabs/puppet/ssl/crl.pem

**logfile (optional)**

The path of the log file.

**loglevel (optional)**

Specify one of the following logging levels: *none*, *trace*, *debug*, *info*,
*warning*, *error*, or *fatal*; the default one is *info*

**log-pcp-access (optional flag)**

Enable PCP access logging; the default is *false*.

**pcp-access-logfile (optional)**

The path of the PCP access log file.

**modules-dir (optional)**

Specify the directory where modules are stored

**modules-config-dir (optional)**

Specify the directory containing the configuration files of modules

**spool-dir (optional)**

The location where the outcome of non-blocking requests will be stored; the
default location is:
 - \*nix: */opt/puppetlabs/pxp-agent/spool*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\var\spool*

Note that if the specified spool directory does not exist, pxp-agent will create
it when starting.

**spool-dir-purge-ttl (optional)**

Automatically delete results subdirectories located in the `spool-dir` directory
that have a `start` timestamp that has expired in respect to the specified TTL.
The TTL value must be an integer with one of the following suffixes:
 - 'm' - minutes
 - 'h' - hours
 - 'd' - days

The default TTL value is "14d" (14 days). Specifying a 0, with any of the above
suffixes, will disable the purge functionality. Note that the purge will take
place when pxp-agent starts and will be repeated every hour or TTL, whichever
is shorter.

**task-cache-dir (optional)**

The location where the tasks are cached; the default location is:
 - \*nix: */opt/puppetlabs/pxp-agent/tasks-cache*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\tasks-cache*

Note that if the specified task-cache directory does not exist, pxp-agent will
create it when starting. It will also be recreated before attempting to download
a task to it in case it was deleted without restarting pxp-agent.

**task-cache-dir-purge-ttl (optional)**

Automatically delete cached tasks located in the `task-cache-dir` directory
that have a `start` timestamp that has expired in respect to the specified TTL.
The TTL value must be an integer with one of the following suffixes:
 - 'm' - minutes
 - 'h' - hours
 - 'd' - days

The default TTL value is "14d" (14 days). Specifying a 0, with any of the above
suffixes, will disable the purge functionality. Note that the purge will take
place when pxp-agent starts and will be repeated every hour or TTL, whichever
is shorter.

**foreground (optional flag)**

Don't become a daemon and execute on foreground on the associated terminal.

**pidfile (optional; only on *nix platforms)**

The path of the PID file; the default is */var/run/puppetlabs/pxp-agent.pid*

## Contributing

Please refer to [this][contributing] document.

[cpp-pcp-client]: https://github.com/puppetlabs/cpp-pcp-client
[leatherman]: https://github.com/puppetlabs/leatherman
[nssm]: https://nssm.cc
[modules_docs]: https://github.com/puppetlabs/pxp-agent/blob/master/modules/README.md
[pcp-broker]: https://github.com/puppetlabs/pcp-broker
[pcp_specs_root]: https://github.com/puppetlabs/pcp-specifications/blob/master/pcp/versions/1.0/README.md
[pxp-module-puppet_docs]: https://github.com/puppetlabs/pxp-agent/blob/master/lib/tests/resources/modules/reverse_valid
[pxp-module-puppet_script]: https://github.com/puppetlabs/pxp-agent/blob/master/modules/pxp-module-puppet.md
[pxp_specs_actions]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/versions/1.0/actions.md
[pxp_specs_request_response]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/versions/1.0/request_response.md
[pxp_specs_root]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/versions/1.0/README.md
[pxp_specs_transaction_status]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/versions/1.0/transaction_status.md
[websocketpp]: https://github.com/zaphoyd/websocketpp
[MinGW-w64]: http://mingw-w64.sourceforge.net/
[Chocolatey]: https://chocolatey.org
[CMake-choco]: https://chocolatey.org/packages/cmake
[7zip-choco]: https://chocolatey.org/packages/7zip.commandline
[MinGW-w64-choco]: https://chocolatey.org/packages/mingw
[Boost-download]: http://sourceforge.net/projects/boost/files/latest/download
[contributing]: CONTRIBUTING.md
