# PXP Agent

This is the agent for the PCP Execution Protocol [(PXP)][1], based on the
the Puppet Communications Protocol [(PCP)][2]. It enables the execution of
[actions][3] on remote nodes, via PXP. The pxp-agent needs to be connected to a
[PCP broker][8] in order to be used; please refer to the documentation below
for how to do that.

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

### Starting unconfigured

If no broker WebSocket URI, SSL key, ca or cert value is supplied, the agent
will still be able to start in unconfigured mode. In this mode no connection
will be established but the process will not terminate.

### Exit code

On success, pxp-agent returns 0. In case of it fails to parse the command line
options or the configuration file, it returns 2. For all other failures,
including invalid options, it returns 1.

## Modules

[Actions][3] are grouped in modules, by which they can be loaded and configured
within pxp-agent. An example of module is given by the [Puppet module][4].

### Modules interface

A module is a file that provides the module metadata and an interface to
execute its actions.

The module metadata is a JSON object containing the following entries:
 - `configuration`: an object that specifies options that may be included in a separate configuration file (see below);
 - `actions`: a JSON array containing specifications for the `input` and `output` of each action.

For a trivial example of module, please refer to the [reverse module][5] used
for testing.

pxp-agent will execute a module to retrieve its metadata (by passing no
argument) and to run a given action (by invoking the action with the input
specified in the [PXP request][6]). Normally when pxp-agent calls a module it
must be executable. When the `interpreter` field is specified in the
configuration file (see below), pxp-agent will use this value to execute the
module instead.

Note that the [transaction status module][7] is implemented natively; there is
no external file for it. Also, `status query` [requests][6] must be *blocking*.

### Modules configuration

Modules can be configured by placing a configuration file in the
`--modules-config-dir`. Config options must be specified in the module metadata
(see above). Module config files are named like `module_name.conf`.


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

** connection-timeout (optional flag) **

Maximum amount of time that may elapse when trying to establish a connection to
the broker in seconds. Setting the value to 0 will disable the timeout.
Defaults to 5 seconds.

**pidfile (optional; only on *nix platforms)**

The path of the PID file; the default is */var/run/puppetlabs/pxp-agent.pid*

[1]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/README.md
[2]: https://github.com/puppetlabs/pcp-specifications/blob/master/pcp/README.md
[3]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/actions.md
[4]: https://github.com/puppetlabs/pxp-agent/blob/master/modules/pxp-module-puppet
[5]: https://github.com/puppetlabs/pxp-agent/blob/master/lib/tests/resources/modules/reverse_valid
[6]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/request_response.md
[7]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/transaction_status.md
[8]: https://github.com/puppetlabs/pcp-broker
[9]: https://nssm.cc
