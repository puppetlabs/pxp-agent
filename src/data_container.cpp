#include "src/data_container.h"
#include "src/errors.h"
#include "src/message.h"

namespace CthunAgent {

DataContainer::DataContainer() {
    document_root_.SetObject();
}

DataContainer::DataContainer(std::string msg) {
    document_root_.Parse(msg.data());

    if (document_root_.HasParseError()) {
        throw message_parse_error { "invalid json" };
    }
}

DataContainer::DataContainer(const rapidjson::Value& value) {
    // Because rapidjson disallows the use of copy constructors we pass
    // the json by const reference and recreate it by explicitly copying
    document_root_.CopyFrom(value, document_root_.GetAllocator());
}

DataContainer::DataContainer(const DataContainer& data) {
    document_root_.CopyFrom(data.document_root_, document_root_.GetAllocator());
}

DataContainer::DataContainer(const DataContainer&& data) {
    document_root_.CopyFrom(data.document_root_, document_root_.GetAllocator());
}

DataContainer& DataContainer::operator=(DataContainer other) {
    std::swap(document_root_, other.document_root_);
    return *this;
}

std::string DataContainer::toString() const {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document_root_.Accept(writer);
    return buffer.GetString();
}

bool DataContainer::validate(valijson::Schema schema, std::vector<std::string>& errors,
                             std::string index_at) {
    if (!index_at.empty()) {
        return Schemas::validate(document_root_[index_at.data()],
                                 schema, errors);
    }

    return Schemas::validate(document_root_, schema, errors);
}

// Private functions

// getValue specialisations
template<>
int DataContainer::getValue<>(const rapidjson::Value& value) const {
    if (value.IsNull()) {
        return 0;
    }
    return value.GetInt();
}

template<>
bool DataContainer::getValue<>(const rapidjson::Value& value) const {
    if (value.IsNull()) {
        return false;
    }
    return value.GetBool();
}

template<>
std::string DataContainer::getValue<>(const rapidjson::Value& value) const {
    if (value.IsNull()) {
        return "";
    }
    return std::string(value.GetString());
}

template<>
double DataContainer::getValue<>(const rapidjson::Value& value) const {
    if (value.IsNull()) {
        return 0.0;
    }
    return value.GetDouble();
}

template<>
DataContainer DataContainer::getValue<>(const rapidjson::Value& value) const {
    if (value.IsNull()) {
        DataContainer container {};
        return container;
    }
    // rvalue return
    DataContainer containter { value };
    return containter;
}

template<>
Message DataContainer::getValue<>(const rapidjson::Value& value) const {
    if (value.IsNull()) {
        Message container {};
        return container;
    }
    // rvalue return
    Message container { value };
    return container;
}

template<>
rapidjson::Value DataContainer::getValue<>(const rapidjson::Value& value) const {
    DataContainer* tmp_this = const_cast<DataContainer*>(this);
    rapidjson::Value v { value, tmp_this->document_root_.GetAllocator() };
    return v;
}

template<>
std::vector<std::string> DataContainer::getValue<>(const rapidjson::Value& value) const {
    std::vector<std::string> tmp {};

    if (value.IsNull()) {
        return tmp;
    }

    for (rapidjson::Value::ConstValueIterator itr = value.Begin();
         itr != value.End();
         itr++) {
        tmp.push_back(itr->GetString());
    }

    return tmp;
}

template<>
std::vector<bool> DataContainer::getValue<>(const rapidjson::Value& value) const {
    std::vector<bool> tmp {};

    if (value.IsNull()) {
        return tmp;
    }

    for (rapidjson::Value::ConstValueIterator itr = value.Begin();
         itr != value.End();
         itr++) {
        tmp.push_back(itr->GetBool());
    }

    return tmp;
}

template<>
std::vector<int> DataContainer::getValue<>(const rapidjson::Value& value) const {
    std::vector<int> tmp {};

    if (value.IsNull()) {
        return tmp;
    }

    for (rapidjson::Value::ConstValueIterator itr = value.Begin();
         itr != value.End();
         itr++) {
        tmp.push_back(itr->GetInt());
    }

    return tmp;
}

template<>
std::vector<double> DataContainer::getValue<>(const rapidjson::Value& value) const {
    std::vector<double> tmp {};

    if (value.IsNull()) {
        return tmp;
    }

    for (rapidjson::Value::ConstValueIterator itr = value.Begin();
         itr != value.End();
         itr++) {
        tmp.push_back(itr->GetDouble());
    }

    return tmp;
}

template<>
std::vector<DataContainer> DataContainer::getValue<>(const rapidjson::Value& value) const {
    std::vector<DataContainer> tmp {};

    if (value.IsNull()) {
        return tmp;
    }

    for (rapidjson::Value::ConstValueIterator itr = value.Begin();
         itr != value.End();
         itr++) {
        DataContainer* tmp_this = const_cast<DataContainer*>(this);
        const rapidjson::Value tmpvalue(*itr, tmp_this->document_root_.GetAllocator());
        DataContainer tmp_data { tmpvalue };
        tmp.push_back(tmp_data);
    }

    return tmp;
}

// setValue specialisations

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, bool new_value) {
    jval.SetBool(new_value);
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, int new_value) {
    jval.SetInt(new_value);
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::string new_value) {
    jval.SetString(new_value.data(), new_value.size(), document_root_.GetAllocator());
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, double new_value) {
    jval.SetDouble(new_value);
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<std::string> new_value ) {
    jval.SetArray();

    for (const auto& value : new_value) {
        // rapidjson doesn't like std::string...
        rapidjson::Value s;
        s.SetString(value.data(), value.size(), document_root_.GetAllocator());
        jval.PushBack(s, document_root_.GetAllocator());
    }
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<bool> new_value ) {
    jval.SetArray();

    for (const auto& value : new_value) {
        rapidjson::Value tmp_val;
        tmp_val.SetBool(value);
        jval.PushBack(tmp_val, document_root_.GetAllocator());
    }
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<int> new_value ) {
    jval.SetArray();

    for (const auto& value : new_value) {
        rapidjson::Value tmp_val;
        tmp_val.SetInt(value);
        jval.PushBack(tmp_val, document_root_.GetAllocator());
    }
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<double> new_value ) {
    jval.SetArray();

    for (const auto& value : new_value) {
        rapidjson::Value tmp_val;
        tmp_val.SetDouble(value);
        jval.PushBack(tmp_val, document_root_.GetAllocator());
    }
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<DataContainer> new_value ) {
    jval.SetArray();

    for (auto value : new_value) {
        rapidjson::Document tmp_value;
        tmp_value.CopyFrom(value.document_root_, document_root_.GetAllocator());
        jval.PushBack(tmp_value, document_root_.GetAllocator());
    }
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, DataContainer new_value ) {
    jval.CopyFrom(new_value.getRaw(), document_root_.GetAllocator());
}

}  // namespace CthunAgent
