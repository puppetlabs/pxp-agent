# Module requirements

### Metadata

When a module is invoked with no arguments, it must write its `metadata` to
stdout.

The `metadata` of a module is a JSON object that contains a number of JSON
schemas for specifying its configuration options and actions. It contains:

 - **configuration**: (optional) schema that describes the module configuration format;
 - **actions**: an array where each item is an object that describes an action implemented by the module (please, refer to the below schema).

The `metadata` schema is:

    {
        "type" : "object",
        "description" : "Metadata format",
        "properties" : {
            "actions" : {
                "type" : "array",
                "description" : "List of actions supported by this module",
                "items" : {
                    "type" : "object",
                    "properties" : {
                        "description" : {
                            "type" : "string",
                            "description" : "Describes an action",
                        },
                        "name" : {
                            "type" : "string",
                            "description" : "Name of the action",
                        },
                        "input" : {
                            "type" : "object",
                            "description" : "Schema for the PXP request arguments",
                        },
                        "results" : {
                            "type" : "object",
                            "description" : "Schema for the action results",
                        },
                    },
                    "required" : ["description", "name", "input", "results"],
                }
            },
            "configuration" : {
                "type" : "object",
                "description" : "Schema for the module configuration"
            },
        },
        "required" : ["actions"],
        "additionalProperties" : false,
    }

Example of `metadata`:

    {
        "description" : "Metadata of the reverse module, used for testing",
        "configuration" : {
            "type" : "object",
            "properties" : { "spam_dir" : { "type" : "string" } },
            "required" : ["spam_dir"],
            "additionalProperties" : false,
        },
        "actions" : [
          { "name" : "string",
            "description" : "Reverses a string",
            "input" : {
                "type" : "object",
                "properties" : { "string" : { "type" : "string" } },
                "required" : [ "input" ],
            },
            "results" : {
                "type" : "object",
                "properties" : { "output" : { "type" : "string" } },
                "required" : [ "output" ],
            },
          },
          { "name" : "hash",
            "description" : "Reverses an element of a hash",
            "input" : {
                "type" : "object",
                "properties" : { "string" : { "type" : "string" } },
                "required" : [ "input" ],
            },
            "results" : {
                "type" : "object",
                "properties" : {
                    "input" : { "type" : "string" },
                    "output" : { "type" : "string" }
                },
            },
                "required" : [ "input", "output" ],
            },
          }
        ],
    }

If the `metadata` contains a `configuration` entry and the configuration file
exists, pxp-agent will validate the module configuration format at startup. If
it's invalid, the actions of such module won't be available.

### Invoking actions

When a module is invoked by passing the name of an action listed in the
`metadata`, such action must be executed.

### Input

When an action is executed, its input arguments should be parsed from stdin.
Actions should expect in input a JSON object with the following format:

    {
        "type" : "object",
        "description" : "The action argument passed via stdin",
        "properties" : {
            "input" : {
                "type" : "object",
                "description" : "PXP request arguments (complies with action's input in metadata)"
            },
            "configuration" : {
                "type" : "object",
                "description" : "Module's configuration (complies with metadata's configuration)"
            },
            "output_files" : {
                "type" : "object",
                "description" : "Contains the paths of files for storing the action output",
                "properties" : {
                    "stdout" : {
                        "type" : "string",
                        "description" : "File for storing the output on stdout"
                    },
                    "stderr" : {
                        "type" : "string",
                        "description" : "File for storing the output on stderr"
                    },
                    "exitcode" : {
                        "type" : "string",
                        "description" : "File for storing the action's exitcode"
                    },
                },
                "required" : ["stdout", "stderr", "exitcode"],
                "additionalProperties" : false,
            }
        },
        "required" : ["input"],
        "additionalProperties" : false,
    }

The `input` and `configuration` entries of an input object comply with the
respective schemas included in the `metadata`; pxp-agent enforces that. Note
that the `configuration` schema is optional; in case it's not included in the
`metadata`, the content of the configuration file will be included in the input
object if its format is valid JSON.

### Output

Once an action completes its execution, its results will be validated by
pxp-agent. That will be done by using the action's `results` schema contained in
the `metadata`. In case the action provides error messages, they will be treated
as free format text and no validation will be performed.

##### Results and error messages on stdout and stderr

In case the input object parsed from stdin does not contain the `output_files`
entry, the action must write its processing results on stdout. Error messages
must go on stderr.

##### Results, error messages, and exit code on file

If the `output_files` object is present in the input, the action must write its
results, error messages, and exit code, in the specified files, respectively:
`stdout`, `stderr`, and `exitcode`.

In this case, pxp-agent guarantees that those files will be read only after the
`exitcode` file is created; the existence of `exitcode` will be used to indicate
the processing completion.

Also, in this case, pxp-agent will discard the output on stdout and stderr
streams.

##### Success or failure?

pxp-agent will report back a successfull action run in case 1) the returned
results comply with the `metadata`'s *results* schema and 2) the exit code is
0.

##### Exit code

The module can use any exit code it wishes to communicate to the requester via
the [PXP Transaction Response][transaction_status]. The only reserved code is
`5`; such code should be used in case the `output_files` entry was included, but
the module failed to write the action's results on file.

[transaction_status]: https://github.com/puppetlabs/pcp-specifications/blob/master/pxp/transaction_status.md
