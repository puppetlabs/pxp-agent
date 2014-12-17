#include "data_container.h"

#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema_parser.hpp>

namespace Cthun {
namespace Agent {

DataContainer::DataContainer(std::string msg) {
    Json::Reader reader;

    if (!reader.parse(msg, message_root_)) {
        throw message_parse_error { reader.getFormattedErrorMessages() };
    }
}

DataContainer::DataContainer(Json::Value value) {
    message_root_ = value;
}

std::string DataContainer::toString() const {
    return message_root_.toStyledString();
}

bool DataContainer::validate(valijson::Schema schema, std::vector<std::string>& errors,
                       std::string index_at) {
    Json::Value root = message_root_;
    if (!index_at.empty()) {
        root = root[index_at];
    }

    return Schemas::validate(root, schema, errors);
}

// Private functions

// getValue specialisations
template<>
int DataContainer::getValue<>(Json::Value value) const {
    return value.asInt();
}

template<>
bool DataContainer::getValue<>(Json::Value value) const {
    return value.asBool();
}

template<>
std::string DataContainer::getValue<>(Json::Value value) const {
    return value.asString();
}

template<>
float DataContainer::getValue<>(Json::Value value) const {
    return value.asFloat();
}

template<>
double DataContainer::getValue<>(Json::Value value) const {
    return value.asDouble();
}

template<>
DataContainer DataContainer::getValue<>(Json::Value value) const {
    return DataContainer { value };
}

template<>
Message DataContainer::getValue<>(Json::Value value) const {
    return Message { value };
}

template<>
Json::Value DataContainer::getValue<>(Json::Value value) const {
    return value;
}

template<>
std::vector<std::string> DataContainer::getValue<>(Json::Value value) const {
    std::vector<std::string> tmp {};

    for (Json::ArrayIndex i = 0u; i < value.size(); i++) {
        tmp.push_back(value.get(i, "").asString());
    }

    return tmp;
}

template<>
std::vector<bool> DataContainer::getValue<>(Json::Value value) const {
    std::vector<bool> tmp {};

    for (Json::ArrayIndex i = 0u; i < value.size(); i++) {
        tmp.push_back(value.get(i, "").asBool());
    }

    return tmp;
}

template<>
std::vector<int> DataContainer::getValue<>(Json::Value value) const {
    std::vector<int> tmp {};

    for (Json::ArrayIndex i = 0u; i < value.size(); i++) {
        tmp.push_back(value.get(i, "").asInt());
    }

    return tmp;
}

template<>
std::vector<float> DataContainer::getValue<>(Json::Value value) const {
    std::vector<float> tmp {};

    for (Json::ArrayIndex i = 0u; i < value.size(); i++) {
        tmp.push_back(value.get(i, "").asFloat());
    }

    return tmp;
}

template<>
std::vector<DataContainer> DataContainer::getValue<>(Json::Value value) const {
    std::vector<DataContainer> tmp {};

    for (Json::ArrayIndex i = 0u; i < value.size(); i++) {
        tmp.push_back(DataContainer { value.get(i, "") });
    }

    return tmp;
}


template<>
std::vector<double> DataContainer::getValue<>(Json::Value value) const {
    std::vector<double> tmp {};

    for (Json::ArrayIndex i = 0u; i < value.size(); i++) {
        tmp.push_back(value.get(i, "").asDouble());
    }

    return tmp;
}

// setValue specialisations

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<std::string> new_value ) {
    Json::Value tmp(Json::arrayValue);
    for (auto value : new_value) {
        tmp.append(Json::Value(value));
    }
    jval = tmp;
}

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<bool> new_value ) {
    Json::Value tmp(Json::arrayValue);
    for (auto value : new_value) {
        // the casting is required
        tmp.append(bool(value));
    }
    jval = tmp;
}

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<int> new_value ) {
    Json::Value tmp(Json::arrayValue);
    for (auto value : new_value) {
        tmp.append(value);
    }
    jval = tmp;
}

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<float> new_value ) {
    Json::Value tmp(Json::arrayValue);
    for (auto value : new_value) {
        tmp.append(value);
    }
    jval = tmp;
}

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<double> new_value ) {
    Json::Value tmp(Json::arrayValue);
    for (auto value : new_value) {
        tmp.append(value);
    }
    jval = tmp;
}

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<DataContainer> new_value ) {
    Json::Value tmp(Json::arrayValue);
    for (auto value : new_value) {
        tmp.append(value.getRaw());
    }
    jval = tmp;
}

template<>
void DataContainer::setValue<>(Json::Value& jval, DataContainer new_value ) {
    jval = new_value.getRaw();
}

}  // namespace Agent
}  // namespace Cthun
