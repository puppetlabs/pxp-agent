# A PXP module for kicking puppet runs

## Installation

Place `modules/pxp-module-puppet` in your pxp-agent's `modules-dir`. This can either be
configured in pxp-agent's configuration file or by starting the agent from the
cli and specifying `--modules-dir`. Make sure that the module is executable.
Also note that pxp-agent's configuration file is, by default, located in:

 - **POSIX**: `/etc/puppetlabs/pxp-agent/pxp-agent.conf`;
 - **Windows**: `C:\ProgramData\PuppetLabs\pxp-agent\etc\pxp-agent.conf`.

## Configuration

The module is configured by placing a `pxp-module-puppet.conf` file in your
pxp-agent's `modules-config-dir`. Note that pxp-agent requires that modules'
configuration files are named as `<module name>.conf`. Also, the
`modules-config-dir` path can either be configured in
`/etc/puppetlabs/pxp-agent/pxp-agent.conf` or by starting the agent from the cli
and specifying `--modules-config-dir`.

The configuration file should have one field:

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
$ sudo echo "{\"input\":{\"env\":[],\"flags\":[\"--noop\"]}, \"configuration\" : {\"puppet_bin\" : \"/opt/puppetlabs/bin/puppet\"}}" | pxp-module-puppet run
```

### Windows (cmd.exe)
```
C:\Program Files\Puppet Labs\Puppet\pxp-agent\modules>echo {"input":{"env":[],"flags":["--noop"]}, "configuration" : {"puppet_bin" : "C\:\\Program Files\\Puppet Labs\\Puppet\\bin\\puppet.bat"}} | pxp-module-puppet.bat run
```

## Output

On successful completion of a Puppet run the module will print a JSON document to
stdout. The JSON document will conform to the following JSON schema:

```
{
    "type" : "object",
    "description" : "pxp-module-puppet output structure",
    "properties" : {
        "version" : { "type" : "number", "description" : "The version of pxp-module-puppet output schema" },
        "report" : {
            "type" : "object",
            "description" : "Selected fields from the `last_run_report.yaml` file",
            "properties" : {
                "kind" : { "type" : "string", "desription" : "The value of `kind` in `last_run_report.yaml`" },
                "time" : { "type" : "string", "desription" : "The value of `time` in `last_run_report.yaml`" },
                "transaction_uuid" : { "type" : "string", "desription" : "The value of `transaction_uuid` in `last_run_report.yaml`" },
                "environment" : { "type" : "string", "desription" : "The value of `environment` in `last_run_report.yaml`" },
                "status" : { "type" : "string", "desription" : "The value of `status` in `last_run_report.yaml`" }
            },
            "required" : ["kind", "time", "transaction_uuid", "environment", "status"],
            "additionalProperties" : false
        },
        "exitcode" : { "type" : "number", "description" : "The exitcode of the Puppet run" },
        "error_type" : { "type" : "string", "description" : "A machine readable error type if one occurred when trying to run Puppet" },
        "error" : { "type" : "string", "description" : "A human readable description of the error if one occurred when trying to run Puppet" }
    },
    "required" : ["version", "report", "exitcode"],
    "additionalProperties" : false
}
```

## Error cases

### Error Types

If a run fails the `error_type` field will be set to one of the following values:

- `invalid_json` pxp-module-puppet was called with invalid json on stdin
- `no_puppet_bin` The executable specified by `puppet_bin` doesn't exist
- `no_last_run_report` last_run_report.yaml doesn't exist
- `invalid_last_run_report` last_run_report.yaml could not be parsed
- `agent_already_running` Puppet agent is already performing a run
- `agent_disabled` Puppet agent is disabled
- `agent_failed_to_start` Puppet agent failed to start
- `agent_exit_non_zero` Puppet agent exited with a non-zero exitcode

### Example error responses


### The `puppet_bin` file does not exist

```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "no_puppet_bin",
    "error" : "Puppet executable '$puppet_bin' does not exist",
    "exitcode" : -1,
    "version" : 1
}
```

### The Puppet agent is busy performing a run when the run action is called

```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "agent_already_running",
    "error" : "Puppet agent is already performing a run",
    "exitcode" : -1,
    "version" : 1
}
```

### The Puppet agent has been disabled on the host the run action was called on

```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "agent_disabled",
    "error" : "Puppet agent is disabled",
    "exitcode" : -1,
    "version" : 1
}
```

### The run action was called with invalid JSON on stdin

```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "invalid_json",
    "error" : "Invalid json parsed on STDIN. Cannot start run action",
    "exitcode" : -1,
    "version" : 1
}
```

### The Puppet run failed to start

```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "agent_failed_to_start",
    "error" : "Failed to start Puppet agent",
    "exitcode" : -1,
    "version" : 1
}
```

### Puppet exited with a non-zero exitcode

```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "agent_exit_non_zero",
    "error" : "Puppet agent exited with a non 0 exitcode",
    "exitcode" : $exitcode,
    "version" : 1
}
```

### last_run_report.yaml doesn't exist after a successful run

```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "no_last_run_report",
    "error" : "$last_run_report doesn't exist",
    "exitcode" : $exitcode,
    "version" : 1
}
```

### last_run_report.yaml exists but isn't valid yaml


```
{
    "report" : {
        "kind" : "unknown",
        "time" : "unknown",
        "transaction_uuid" : "unknown",
        "environment" : "unknown",
        "status" : "unknown"
    },
    "error_type" : "invalid_last_run_report",
    "error" : "$last_run_report could not be loaded: $error",
    "exitcode" : $exitcode,
    "version" : 1
}
```
