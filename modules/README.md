# A PXP module for kicking puppet runs

## Installation

Place `modules/puppet` in your pxp-agent's `modules-dir`. This can either be
configured in `/etc/puppetlabs/pxp-agent/pxp-agent.cfg` or by starting the agent
from the cli and specifying `--modules-dir`. Make sure that the module is executable.

## Configuration

The module is configured by placing a `puppet.cfg` file in your pxp-agent's
`modules-config-dir`. This can either be configured in
`/etc/puppetlabs/pxp-agent/pxp-agent.cfg` or by starting the agent from the cli
and specifying `--modules-config-dir`.

The configuration file will have one field:

* `puppet_bin` : The location of the puppet executable on the system pxp-agent is running on.

### Example

```
{
    "puppet_bin" : "/opt/puppetlabs/bin/puppet"
}
```

## Actions and Action Arguments

The module responds to two actions.

- `metadata` : This is used by the pxp-agent and doesn't need to be called by consumers
- `run` : This action will attempt to trigger a Puppet run

The `run` action has two required parameters:

* `env` : An array of strings that match ".*=.*". These are environment variables
that will be set before running Puppet.
* `flags` : An array of strings that match "--.*" or "--.*=.*". These are cli flags
that will be passed to the Puppet run.

## Testing in isolation

You can test the module without running a pxp-agent. Make sure the module is
executable and you can run it like:

```
$ sudo echo "{\"params\":{\"env\":[],\"flags\":[\"--noop\"]}, \"config\" : {\"puppet_bin\" : \"/opt/puppetlabs/bin/puppet\"}}" | puppet run
```

### NOTE

This manner of execution is subject to change on completion of
[CTH-299](https://tickets.puppetlabs.com/browse/CTH-299)

## Output

On successful completion of a Puppet run the module will print a JSON document to
stdout. The JSON document will have the following fields.

- `kind` : The value of `kind` in last_run_report.yaml
- `time` : The value of `time` in last_run_report.yaml
- `transaction_uuid` : The value of `transaction_uuid` in last_run_report.yaml
- `environment` : The value of `environment` in last_run_report.yaml
- `status` : The value of `status` in last_run_report.yaml
- `error` : A string containing an error description if one occurred when trying to run Puppet
- `exitcode` : The exitcode of the Puppet run

### Error cases

### `puppet_bin` configuration value hasn't been set

Output will be

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "puppet_bin configuration value not set",
    "exitcode" : -1
}
```

### The `puppet_bin` file does not exist

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "Puppet executable '$puppet_bin' does not exist",
    "exitcode" : -1
}
```

### The Puppet agent is busy performing a run when the run action is called

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "Puppet is already running",
    "exitcode" : -1
}
```

### The Puppet agent has been disabled on the host the run action was called on

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "Puppet is disabled",
    "exitcode" : -1
}
```

### The run action was called with invalid JSON on stdin

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "Invalid json parsed on STDIN",
    "exitcode" : -1
}
```

### The Puppet run failed to start

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "Failed to start Puppet",
    "exitcode" : -1
}
```

### Puppet exited with a non-zero exitcode

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "Puppet exited with a non 0 exitcode",
    "exitcode" : $exitcode
}
```

### last_run_report.yaml doesn't exist after a successful run

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "$last_run_report.yaml doesn't exist",
    "exitcode" : $exitcode
}
```

### last_run_report.yaml exists but isn't valid yaml


```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error" : "$last_run_report.yaml isn't valid yaml",
    "exitcode" : exitcode
}
```
