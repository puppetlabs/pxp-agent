# Cthun Agent

This is the experimental agent for the Cthun command and control framework.

## Building from source

### Build requirements
 - a C++11 compiler (clang/gcc 4.7)
 - gnumake
 - cmake
 - boost
 - cfacter (0.2.0 after commit 3e948eb)
 - ruby (2.0 and newer)

Build with make and make install

## Configuring the agent

The cthun agent can be configured using a config file or supplying arguments on the command line.

The agent will first look in the user's home directory for *.cthun-agent* and then in
*/etc/cthun/agent.cfg. A different config file can be specified by passing the --config-file flag

The config files use the JSON format. Options must be specified as entries of a
single JSON object. Example:

```
{
    "server" : "wss://127.0.0.1:8090/cthun/",
    "ca" : "/Users/psy/work/cthun-agent/test-resources/ssl/ca/ca_crt.pem",
    "cert" : "/Users/psy/work/cthun-agent/test-resources/ssl/certs/0005_agent_crt.pem",
    "key" : "/Users/psy/work/cthun-agent/test-resources/ssl/certs/0005_agent_key.pem"
}
```

## Config options

The Cthun agent has the following configuration options

**config-file (optional)**

Specify which config file to use.

**server (required)**

The Cthun server you wish to connect the agent to, example wws://192.168.0.1:8061/cthun/

**ca (required)**

The location of your CA certificate, example /etc/puppet/ssl/ca/ca_crt.pem

**cert (required)**

The location of the cthun-agent certificate, example /etc/puppet/ssl/certs/bob_crt.pem

**key (required)**

The location of the cthun-agent's private key, example /etc/puppet/ssl/certs/bob_key.pem

**logfile (optional)**

Location of the file where log output should be written to, example /var/log/cthun/agent.log.
If logfile is not configured output will be logged to the console

**spool-dir (optional)**

The location where the outcome of non-blocking requests will be stored; the
default location is /tmp/cthun-agent/

### Starting the agent

The agent can be started by running
```
cthun-agent
```
