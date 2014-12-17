#include "message.h"
#include <iostream>

namespace Agent {

Message::Message(std::string msg) {
    Json::Reader reader;

    if (!reader.parse(msg, message_root_)) {
        throw message_parse_error { reader.getFormattedErrorMessages() };
    }
}

std::string Message::toString() {
    return message_root_.toStyledString();
}

// Private functions

// get() specialisations
template<>
int Message::getValue<>(Json::Value value) {
    return value.asInt();
}

template<>
bool Message::getValue<>(Json::Value value) {
    return value.asBool();
}

template<>
std::string Message::getValue<>(Json::Value value) {
    return value.asString();
}

}  // namespace Agent
