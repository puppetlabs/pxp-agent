
# PXP agent architecture

map of name:module, possibly factory style from peg

Module abstraction

    class Module {
        std::string name;
        bool invoke(const CthunMessage& request, CthunMessage& response);
        Schema actions;
    }


## External modules

Load from libdir

Will have 1 required action, metadata, takes no parameters, yields a
json description of the actions it has.  Parameters subkey should be a
json-schema document the deamon will validate:

    ./package metadata
    {
        "description": "Manage packages",
        "actions": [
            {
                "name": "install",
                "input": {
                    "type": "object",
                    "properties": {
                        "name": {
                            "type": "string"
                        },
                        "version": {
                            "type": "string"
                        },
                    },
                    "required": [ "name" ]
                },
                "output": {
                    "type": "object",
                    "properties": {
                        "success": {
                            "type": "boolean"
                        }
                    },
                    "required": [ "success" ]
                }
            }
         ]
     }


Invocation of an action will supply parameters as a document on stdin

    echo '{ "name": "nmap" }' | ./package install
    { "success": true }
