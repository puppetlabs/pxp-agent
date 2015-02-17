#include "src/data_parser.h"
#include "src/errors.h"

#include <valijson/schema.hpp>

namespace CthunAgent {

DataContainer getJson_(const MessageChunk& chunk) {
    // Get the schemas once for all...
    static const auto envelope_schema = Schemas::getEnvelopeSchema();
    static const auto data_schema = Schemas::getDataSchema();

    // Get the chunk type (envelope, data, debug)
    auto type = chunk.descriptor & ChunkDescriptor::TYPE_MASK;

    // Parse the JSON content
    DataContainer json_content { chunk.content };

    // Validation errors
    std::vector<std::string> errors;

    if (type == ChunkDescriptor::ENVELOPE) {
        // Validate envelope
        if (!json_content.validate(envelope_schema, errors)) {
            std::string error_message { "envelope schema validation failed:\n" };
            for (auto error : errors) {
                error_message += error + "\n";
            }
            throw message_validation_error { error_message };
        }

        // Check the specified data schema
        if (json_content.get<std::string>("data_schema") != CTHUN_REQUEST_SCHEMA) {
            throw message_validation_error { "data is not of cnc request schema" };
        }
    } else if (type == ChunkDescriptor::DATA) {
        // Validate data
        if (!json_content.validate(data_schema, errors)) {
            std::string error_message { "data schema validation failed:\n" };
            for (auto error : errors) {
                error_message += error + "\n";
            }
            throw message_validation_error { error_message };
        }
    }

    return json_content;
}

//
// api
//

DataContainer DataParser::parseAndValidateChunk(const MessageChunk& chunk,
                                                CthunContentFormat format) {
    if (format == CthunContentFormat::json) {
        return getJson_(chunk);
    }

    // So far we only handle json content
    throw message_validation_error { "unknown format" };
}

}  // namespace CthunAgent
