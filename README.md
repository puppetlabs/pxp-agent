# PXP Agent

This is the agent for the PCP Execution Protocol [(PXP)][1], based on the
the Puppet Communications Protocol [(PCP)][2]. It enables the execution of
[actions][3] on remote nodes, via PXP.

The PCP interface is provided by [cpp-pcp-client][10] which, in turn, relies on
[websocket++][11].

pxp-agent needs to be connected to a [PCP broker][8] to operate; please refer to
the documentation below for how to do that.

## Building from source

### Build requirements
 - a C++11 compiler (clang/gcc 4.7)
 - gnumake
 - cmake
 - boost
 - ruby (2.0 and newer)

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

pxp-agent relies on [nssm][9] to execute as a service. In case `--foreground` is
unflagged, a mutex-based mechanism will prevent multiple instances of pxp-agent.
Note that no PID file will be created.

### Exit code

In POSIX, when the daemon is successfully instantiated, the parent process
returns 0. In case of a daemonization failure, it returns 4.

In case of it fails to parse the command line options or the configuration file,
pxp-agent returns 2.

In case of invalid configuration, say an option is set to an invalid value, it
returns 3.

For all other failures it returns 1.

## Modules

[Actions][3] are grouped in modules, by which they can be loaded and configured
within pxp-agent. An example of module is given by the [Puppet module][4];
a trivial one is the [reverse module][5] that is used for testing.

### Modules interface

A module is a file that provides an interface to retrieve information about
its actions (we call such information metadata - it's a set of JSON schemas) and
to execute actions.

The metadata is used by pxp-agent to acquire knowledge about the module's
actions and validate itss configuration. For each action, the metadata specifies
the format of the input arguments and the output results. Please refer to
[this document][12] for more details on requirements for modules.

To run a given action, pxp-agent invokes the module with the action
name. The input specified in the [PXP request][6] and other parameters will be
then passed to the module via stdin. Normally pxp-agent invokes a module
directly, as an executable.

Note that the [transaction status module][7] is implemented natively; there is
no module file for it. Also, as a side note, `status query` requests must
be of [blocking][6].

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

Note that you have to specify the WebSocket secure URL of the [PCP broker][8]
in order to establish the WebSocket connection on top of which the PCP
communication will take place.

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

[1]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/README.md
[2]: https://github.com/puppetlabs/pcp-specifications/blob/master/pcp/README.md
[3]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/actions.md
[4]: https://github.com/puppetlabs/pxp-agent/blob/master/modules/pxp-modules-puppet.md
[5]: https://github.com/puppetlabs/pxp-agent/blob/master/lib/tests/resources/modules/reverse_valid
[6]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/request_response.md
[7]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/transaction_status.md
[8]: https://github.com/puppetlabs/pcp-broker
[9]: https://nssm.cc
[10]: https://github.com/puppetlabs/cpp-pcp-client
[11]: https://github.com/zaphoyd/websocketpp
[12]: https://github.com/puppetlabs/pxp-agent/blob/master/modules/README.md
