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

The PXP agent is configured with a config file. The values in the config file can be
overridden by supplying arguments on the command line.

The agent will look for the default config file */etc/puppetlabs/pxp-agent/pxp-agent.cfg*
A different config file can be specified by passing the --config-file flag

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

## Config options

The PXP agent has the following configuration options

**config-file (required)**

Specify which config file to use.

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

**spool-dir (optional)**

The location where the outcome of non-blocking requests will be stored; the
default location is */tmp/pxp-agent/*

**modules-dir (optional)**

Specify the directory where modules are stored

**modules-config-dir (optional)**

Specify the directory containing the configuration files of modules

### Starting the agent

The agent can be started by running
```
pxp-agent
```
in the ./build/bin directory
