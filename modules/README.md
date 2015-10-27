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

### Posix
```
$ sudo echo "{\"params\":{\"env\":[],\"flags\":[\"--noop\"]}, \"config\" : {\"puppet_bin\" : \"/opt/puppetlabs/bin/puppet\"}}" | pxp-module-puppet run
```

### Windows (cmd.exe)
```
C:\Program Files\Puppet Labs\Puppet\pxp-agent\modules>echo {"params":{"env":[],"flags":["--noop"]}, "config" : {"puppet_bin" : "C\:\\Program Files\\Puppet Labs\\Puppet\\bin\\puppet.bat"}} | pxp-module-puppet.bat run
```

## Output

On successful completion of a Puppet run the module will print a JSON document to
stdout. The JSON document will have the following fields.

- `kind` : The value of `kind` in last_run_report.yaml
- `time` : The value of `time` in last_run_report.yaml
- `transaction_uuid` : The value of `transaction_uuid` in last_run_report.yaml
- `environment` : The value of `environment` in last_run_report.yaml
- `status` : The value of `status` in last_run_report.yaml
- `error_type` : A string containing the machine readable error type
- `error` : A string containing an error description if one occurred when trying to run Puppet
- `exitcode` : The exitcode of the Puppet run

## Error cases

### Error Types

If a run fails the `"error_type"` field will be set to one of the following values:

- `"invalid_json"` pxp-module-puppet was called with invalid json on stdin
- `"no_puppet_bin"` The executable specified by `puppet_bin` doesn't exist
- `"no_last_run_report"` last_run_report.yaml doesn't exist
- `"invalid_last_run_report"` last_run_report.yaml could not be parsed
- `"agent_already_running"` Puppet agent is already performing a run
- `"agent_disabled"` Puppet agent is disabled
- `"agent_failed_to_start"` Puppet agent failed to start
- `"agent_exit_non_zero"` Puppet agent exited with a non-zero exitcode

### Example error responses


### The `puppet_bin` file does not exist

```
{
    "kind" : "unknown",
    "time" : "unknown",
    "transaction_uuid" : "unknown",
    "environment" : "unknown",
    "status" : "unknown",
    "error_type" : "no_puppet_bin",
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
    "error_type" : "agent_already_running",
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
    "error_type" : "agent_disabled",
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
    "error_type" : "invalid_json",
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
    "error_type" : "agent_failed_to_start",
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
    "error_type" : "agent_exit_non_zero",
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
    "error_type" : "no_last_run_report",
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
    "error_type" : "invalid_last_run_report",
    "error" : "$last_run_report.yaml isn't valid yaml",
    "exitcode" : exitcode
}
```
