#ifndef CTHUN_SRC_DATA_CONTAINER_H_
#define CTHUN_SRC_DATA_CONTAINER_H_

#include "src/schemas.h"
#include "src/errors.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/allocators.h>

#include <iostream>

namespace CthunAgent {

// Data abstraction overlaying the raw JSON objects
// TODO(ploubser): We inherit this class to implement the current Message
// implementation. This will change when the Message becomes more formal
// and segmented but at the time of CTH-108 this allows us to hide the json
// implementation


// Usage:
// NOTE SUPPORTED SCALARS
// int, float, double, bool, std::string, nullptr
//
// To set a key to a scalar value in object x
//    x.set<int>(1, "foo");
//    x.set<std::string>("bar", "foo");
//
// NOTE THAT VALUE IS FIRST, KEY IS SECOND
//
// To set a nested key to a scalar value in object x
//    x.set<bool>(true, "foo", "bar", "baz");
//
// NOTE THAT LOCATION IN STUCTURE IS A VARAIDIC ARGUMENT
//
// To set a key to a vector value in object x
//    std::vector<int> tmp { 1, 2, 3 };
//    x.set<std::vector<int>>(tmp, "foo");
//
// To get a scalar value from a key in object x
//    x.get<std::string>("foo");
//    x.get<int>("bar");
//
// To get a vector from a key in object x
//    x.get<std::vector<float>>("foo");
//
// To get a result object (json object) from object x
//    x.get<Message>("foo");
//    x.get<Data>("foo");
//
// To get a null value from a key in object x
//    x.get<std::string>("foo") == "";
//    x.get<int>("foo") == 0;
//    x.get<bool>("foo") == false;
//    x.get<float>("foo") == 0.0f;
//    x.get<double>("foo") == 0.0;
//
// To get a json string representation of object x
//    x.toString();
//
// To validate an object x
//    valijson::Schema schema { y.getSchema() };
//    std::vector<std::string> errors;
//    x.validate(schema, errors);
//
//    or
//
//    x.validate(schema, errors, "data");
//    // TODO(ploubser): Make validate variadic and add tests
//
// To check if a key is set in object x
//    x.includes("foo");
//    x.includes("foo", "bar", "baz");

class DataContainer {
  public:
    DataContainer();
    explicit DataContainer(std::string msg);
    explicit DataContainer(const rapidjson::Value& value);
    DataContainer(const DataContainer& data);
    DataContainer(const DataContainer&& data);
    DataContainer& operator=(DataContainer other);

    std::string toString() const;

    bool validate(valijson::Schema schema, std::vector<std::string>& errors,
                  std::string index_at = "");

    bool includes(std::string first) {
        return includes_(document_root_, first.data());
    }

    template <typename ... Args>
    bool includes(std::string first, Args... rest) {
        return includes_(document_root_, first.data(), rest...);
    }

    rapidjson::Document getRaw() {
        rapidjson::Document tmp;
        rapidjson::Document::AllocatorType& a = document_root_.GetAllocator();
        tmp.CopyFrom(document_root_, a);
        return std::move(document_root_);
    }

    template <typename T>
    T get(std::string first) const {
        const rapidjson::Value& v = document_root_;
        return get_<T>(v, first.data());
    }

    template <typename T, typename ... Args>
    T get(std::string first, Args ... rest) const {
        const rapidjson::Value& v = document_root_;
        return get_<T>(v, first.data(), rest...);
    }

    template <typename T>
    void set(T new_value, std::string first) {
        set_<T>(document_root_, new_value, first.data());
    }

    template <typename T, typename ... Args>
    void set(T new_value, std::string first, Args ... rest) {
        set_<T>(document_root_, new_value, first.data(), rest...);
    }

  private:
    rapidjson::Document document_root_;

    template <typename ... Args>
    bool includes_(const rapidjson::Value& jval, const char * first) {
        if (jval.HasMember(first)) {
            return true;
        } else {
            return false;
        }
    }

    template <typename ... Args>
    bool includes_(const rapidjson::Value& jval, const char * first, Args ... rest) {
        if (jval[first].IsObject()) {
            return includes_(jval[first], rest...);
        } else {
            return false;
        }
    }

    template <typename T>
    T get_(const rapidjson::Value& jval, const char * first) const {
        if (!jval.HasMember(first)) {
            return getValue<T>(rapidjson::Value(rapidjson::kNullType));
        }

        return getValue<T>(jval[first]);
    }

    template <typename T, typename ... Args>
    T get_(const rapidjson::Value& jval, const char * first, Args ... rest) const {
        if (!(jval.HasMember(first) && jval[first].IsObject())) {
            return getValue<T>(rapidjson::Value(rapidjson::kNullType));
        }

        return get_<T>(jval[first], rest...);
    }

    template <typename T, typename ... Args>
    void set_(rapidjson::Value& jval, T new_val, const char * first) {
        if (!jval.HasMember(first)) {
            jval.AddMember(rapidjson::Value(first, document_root_.GetAllocator()).Move(),
                           rapidjson::Value().Move(),
                           document_root_.GetAllocator());
        }

        setValue<T>(jval[first], new_val);
    }

    template <typename T, typename ... Args>
    void set_(rapidjson::Value& jval, T new_val, const char * first, Args ... rest) {
        if (jval.HasMember(first) && !jval[first].IsObject()) {
            throw message_index_error { "invalid message index supplied" };
        }

        if (!jval.HasMember(first)) {
            jval.AddMember(rapidjson::Value(first, document_root_.GetAllocator()).Move(),
                           rapidjson::Value(rapidjson::kObjectType).Move(),
                           document_root_.GetAllocator());
        }

        set_<T>(jval[first], new_val, rest...);
    }

    template<typename T>
    T getValue(const rapidjson::Value& value) const;


    template<typename T>
    void setValue(rapidjson::Value& jval, T new_value);
};


template<typename T>
T DataContainer::getValue(const rapidjson::Value& Value) const {
    throw data_type_error { "invalid type for DataContainer" };
}

template<typename T>
void DataContainer::setValue(rapidjson::Value& jval, T new_value) {
    throw data_type_error { "invalid type for DataContainer" };
}

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::string new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, bool new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, int new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, double new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<std::string> new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<bool> new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<int> new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<double> new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, std::vector<DataContainer> new_value);

template<>
void DataContainer::setValue<>(rapidjson::Value& jval, DataContainer new_value);

// TODO(ploubser): This becomes the current message imp
// when the message structure changes
class Message : public DataContainer {
  public:
    Message() {}
    explicit Message(std::string msg) : DataContainer(msg) {}
    explicit Message(const rapidjson::Value& value) : DataContainer(value) {}
    Message(const Message& msg) : DataContainer(msg) {}
    Message(const Message&& msg) : DataContainer(msg) {}
    Message& operator=(Message other) {
        DataContainer::operator=(other);
        return *this;
    }
};

}  // namespace CthunAgent

#endif  // CTHUN_SRC_DATA_CONTAINER_H_
