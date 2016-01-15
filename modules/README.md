# Module requirements

### Metadata

The `metadata` of a module is a JSON object that contains a number of JSON
schemas for specifying configuration options and actions. It contains:

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

When a module is invoked with no arguments it must write its `metadata` to
stdout.

When a module is invoked by passing the name of an action listed in the
`metadata`, such action must be executed.

### Input / output

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

Each action should write its processing results on stdout. Error messages should
go on stderr.

If the input object parsed from stdin contains the `output_files` object, the
action must ALSO write its results, error messages, and exit code in the
specified files (respectively: `stdout`, `stderr`, and `exitcode`). In this
case, pxp-agent guarantees that those files will be read only after the
`exitcode` file is created; the existence of `exitcode` will be used to indicate
the action completion.

Once an action completes its execution, its results will be validated by
pxp-agent by using the relative `results` schema contained in the `metadata`.
The output on stderr will be treated as free format text.
