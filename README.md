# PXP Agent

This is the agent for the PCP Execution Protocol [(PXP)][pxp_specs_root], based
on the the Puppet Communications Protocol [(PCP)][pcp_specs_root]. It enables
the execution of [actions][pxp_specs_actions] on remote nodes.

The PCP interface is provided by [cpp-pcp-client][cpp-pcp-client] which, in
turn, relies on [websocket++][websocketpp].

pxp-agent needs to be connected to a [PCP broker][pcp-broker] to operate; please
refer to the documentation below for how to do that.

## Building from source

### Build requirements
 - a C++11 compiler (clang/gcc 4.7)
 - gnumake
 - cmake
 - boost
 - ruby (2.0 and newer)
 - [leatherman][leatherman](0.3.5 or newer)
 - [cpp-pcp-client][cpp-pcp-client] (master)

Build with make and make install

## Starting the agent

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

### *nix

In case `--foreground` is unflagged, pxp-agent will start as a daemon and its
PID will be stored in */var/run/puppetlabs/pxp-agent.pid*, or in another
location if specified by `--pidfile`. pxp-agent will rely on such file to
prevent multiple daemon instances from executing at once. The PID file will be
removed if the daemon is stopped by using one of SIGINT, SIGTERM, or SIGQUIT
signals.

### Windows

pxp-agent relies on [nssm][nssm] to execute as a service. In case `--foreground`
is unflagged, a mutex-based mechanism will prevent multiple instances of
pxp-agent. Note that no PID file will be created.

### Exit code

In POSIX, when the daemon is successfully instantiated, the parent process
returns 0. In case of a daemonization failure, it returns 4.

In case of it fails to parse the command line options or the configuration file,
pxp-agent returns 2.

In case of invalid configuration, say an option is set to an invalid value, it
returns 3.

For all other failures it returns 1.

## Modules

[Actions][pxp_specs_actions] are grouped in modules, by which they can be loaded
and configured within pxp-agent. An example of module is given by the
[Puppet module][pxp-module-puppet_script]; a trivial one is the
[reverse module][pxp-module-puppet_docs] that is used for testing.

### Modules interface

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

### Modules configuration

Modules can be configured by placing a configuration file in the
`--modules-config-dir` named like `<module_name>.conf`. The content of a
configuration file must be in JSON format and conform with the `configuration`
entry of the module's metadata.

## Configuring the agent

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
    "broker-ws-uri" : "wss://127.0.0.1:8142/pcp/",
    "ssl-key" : "/etc/puppetlabs/puppet/ssl/private_keys/myhost.net.pem",
    "ssl-ca-cert" : "/etc/puppetlabs/puppet/ssl/certs/ca.pem",
    "ssl-cert" : "/etc/puppetlabs/puppet/ssl/certs/myhost.net.pem"
}
```

Note that you have to specify the WebSocket secure URL of the
[PCP broker][pcp-broker] in order to establish the WebSocket connection on top
of which the PCP communication will take place.

### Logging

By default, log messages will be written to:
 - \*nix: */var/log/puppetlabs/pxp-agent/pxp-agent.log*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\var\log\pxp-agent.log*.

You can specify a different file with the `--logfile` option.

When running in foreground mode, it is possible to display log messages on
console by using an hyphen instead of a file path: `--logfile -`.

The default log level is `info`. You can specify a different log level by
using the `--loglevel` option with one of the following strings: `none`,
`trace`, `debug`, `info`, `warning`, `error`, `fatal`.

### List of all configuration options

The PXP agent has the following configuration options

**config-file (optional)**

Specify which config file to use.

**broker-ws-uri (required to connect)**

The WebSocket URI of the PXP broker you wish to connect the agent to, example
*wss://192.168.0.1:8142/pxp/*

**connection-timeout (optional flag)**

Maximum amount of time that may elapse when trying to establish a connection to
the broker in seconds. Defaults to 5 seconds.

**ssl-ca-cert (required to connect)**

The location of your SSL Certificate Authority certificate, example
/etc/puppet/ssl/ca/ca_crt.pem

**ssl-cert (required to connect)**

The location of the pxp-agent SSL certificate, example /etc/puppet/ssl/certs/bob_crt.pem

**ssl-key (required to connect)**

The location of the pxp-agent's SSL private key, example /etc/puppet/ssl/certs/bob_key.pem

**logfile (optional)**

The path of the log file.

**loglevel (optional)**

Specify one of the following logging levels: *none*, *trace*, *debug*, *info*,
*warning*, *error*, or *fatal*; the default one is *info*

**modules-dir (optional)**

Specify the directory where modules are stored

**modules-config-dir (optional)**

Specify the directory containing the configuration files of modules

**spool-dir (optional)**

The location where the outcome of non-blocking requests will be stored; the
default location is:
 - \*nix: */opt/puppetlabs/pxp-agent/spool*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\var\spool*

**foreground (optional flag)**

Don't become a daemon and execute on foreground on the associated terminal.

**pidfile (optional; only on *nix platforms)**

The path of the PID file; the default is */var/run/puppetlabs/pxp-agent.pid*

[cpp-pcp-client]: https://github.com/puppetlabs/cpp-pcp-client
[leatherman]: https://github.com/puppetlabs/leatherman
[nssm]: https://nssm.cc
[modules_docs]: https://github.com/puppetlabs/pxp-agent/blob/master/modules/README.md
[pcp-broker]: https://github.com/puppetlabs/pcp-broker
[pcp_specs_root]: https://github.com/puppetlabs/pcp-specifications/blob/master/pcp/README.md
[pxp-module-puppet_docs]: https://github.com/puppetlabs/pxp-agent/blob/master/lib/tests/resources/modules/reverse_valid
[pxp-module-puppet_script]: https://github.com/puppetlabs/pxp-agent/blob/master/modules/pxp-modules-puppet.md
[pxp_specs_actions]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/actions.md
[pxp_specs_request_response]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/request_response.md
[pxp_specs_root]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/README.md
[pxp_specs_transaction_status]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/transaction_status.md
[websocketpp]: https://github.com/zaphoyd/websocketpp
