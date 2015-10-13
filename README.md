# PXP Agent

This is the agent for the Puppet Remote Execution Protocol (PXP), based on the
the Puppet Communications Protocol (PCP).

## Building from source

### Build requirements
 - a C++11 compiler (clang/gcc 4.7)
 - gnumake
 - cmake
 - boost
 - ruby (2.0 and newer)

Build with make and make install

## Configuring the agent

The PXP agent is configured with a config file. The values in the config file
can be overridden by supplying arguments on the command line.

The agent will look for the default config file in:
 - \*nix: */etc/puppetlabs/pxp-agent/pxp-agent.conf*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\etc\pxp-agent.conf*

A different config file can be specified by passing the `--config-file` option.
In case the `--config-file` option is not used and the default config file does
not exist, the agent will start anyway in unconfigured mode; but will not
attempt to connect to any PCP server.

The config files use the JSON format. Options must be specified as entries of a
single JSON object. Example:

```
{
    "server" : "wss://127.0.0.1:8090/pcp/",
    "key" : "/etc/puppetlabs/puppet/ssl/private_keys/myhost.net.pem",
    "ca" : "/etc/puppetlabs/puppet/ssl/certs/ca.pem",
    "cert" : "/etc/puppetlabs/puppet/ssl/certs/myhost.net.pem"
}
```

## Configuring modules

Modules can be configured by placing a configuration file in the
`--modules-config-dir`. Config options must be specified in the metadata returned
from executing the module with no arguments before they can be specified. Module
config files are named like `module_name.conf`.

Module configuration files may specify the `interpreter` field. Normally when
pxp-agent calls a module it must be executable. When the `interpreter` field
is specified pxp-agent will use this value to execute the module instead. For
example:

```
{
    "interpreter" : "/opt/puppetlabs/puppet/bin/ruby"
}
```

## Daemon

The agent will execute as a daemon unless the `--foreground` flag is specified.
During its execution, the daemon PID will be stored in:
 - \*nix: */var/run/puppetlabs/pxp-agent.pid*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\var\run\pxp-agent.pid*

## Logging

By default, log messages will be writted to the pxp-agent.log file in:
 - \*nix: */var/log/puppetlabs/pxp-agent*
 - Windows: *C:\ProgramData\PuppetLabs\pxp-agent\var\log*

You can specify a different directory with the `--logdir` flag.

The default log level is `info`. You can specify a different log level by
using the `--loglevel` option with one of the following strings: `none`,
`trace`, `debug`, `info`, `warning`, `error`, `fatal`.

### List of all configuration options

The PXP agent has the following configuration options

**config-file (optional)**

Specify which config file to use.

**foreground (optional flag)**

Don't become a daemon and execute on foreground on the associated terminal.

**server (required)**

The PXP server you wish to connect the agent to, example wws://192.168.0.1:8061/pxp/

**ca (required)**

The location of your CA certificate, example /etc/puppet/ssl/ca/ca_crt.pem

**cert (required)**

The location of the pxp-agent certificate, example /etc/puppet/ssl/certs/bob_crt.pem

**key (required)**

The location of the pxp-agent's private key, example /etc/puppet/ssl/certs/bob_key.pem

**logfile (optional)**

Location of the file where log output should be written to, example /var/log/pxp/agent.log.
If logfile is not configured output will be logged to the console

**loglevel (optional)**

Specify one of the following logging levels: *none*, *trace*, *debug*, *info*,
*warning*, *error*, or *fatal*; the default one is *info*

**console-logger (optional flag)**

Display logging messages on the associated terminal; requires `--foreground`

**spool-dir (optional)**

The location where the outcome of non-blocking requests will be stored; the
default location is */tmp/pxp-agent/*

**modules-dir (optional)**

Specify the directory where modules are stored

**modules-config-dir (optional)**

Specify the directory containing the configuration files of modules

## Starting the agent

The agent can be started by running
```
pxp-agent
```
in the ./build/bin directory
