#ifndef CTHUN_AGENT_SRC_DATA_CONTAINER_H_
#define CTHUN_AGENT_SRC_DATA_CONTAINER_H_

#include "src/agent/schemas.h"

#include <json/json.h>

namespace Cthun {
namespace Agent {

/// Error thrown when a message string is invalid
class message_parse_error : public std::runtime_error {
  public:
    explicit message_parse_error(std::string const& msg) :
        std::runtime_error(msg) {}
};

/// Error thrown when a nested message index is invalid
class message_index_error : public std::runtime_error {
  public:
    explicit message_index_error(std::string const& msg) :
        std::runtime_error(msg) {}
};

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
    explicit DataContainer(){}
    explicit DataContainer(std::string msg);
    explicit DataContainer(Json::Value value);

    std::string toString() const;

    bool validate(valijson::Schema schema, std::vector<std::string>& errors,
                  std::string index_at = "");

    bool includes(std::string first) {
        return includes_(message_root_, first);
    }

    Json::Value getRaw() {
        return message_root_;
    }

    template <typename ... Args>
    bool includes(std::string first, Args... rest) {
        return includes_(message_root_, first, rest...);
    }

    template <typename T>
    T get(std::string first) const {
        return get_<T>(message_root_, first);
    }

    template <typename T, typename ... Args>
    T get(std::string first, Args ... rest) const {
        return get_<T>(message_root_, first, rest...);
    }

    template <typename T>
    void set(T new_value, std::string first) {
        set_<T>(message_root_, new_value, first);
    }

    template <typename T, typename ... Args>
    void set(T new_value, std::string first, Args ... rest) {
        set_<T>(message_root_, new_value, first, rest...);
    }

  private:
    Json::Value message_root_ {};

    template <typename ... Args>
    bool includes_(Json::Value jval, std::string first) {
        if (jval.isMember(first)) {
            return true;
        } else {
            return false;
        }
    }

    template <typename ... Args>
    bool includes_(Json::Value jval, std::string first, Args ... rest) {
        if (jval.isMember(first)) {
            return includes_(jval[first], rest...);
        } else {
            return false;
        }
    }

    template <typename T, typename ... Args>
    T get_(Json::Value jval, std::string first, Args ... rest) const {
        if (!(jval.isObject() && jval.isMember(first))) {
            return getValue<T>(Json::nullValue);
        }

        return get_<T>(jval[first], rest...);
    }

    template <typename T>
    T get_(Json::Value jval, std::string first) const {
        if (!(jval.isObject() && jval.isMember(first))) {
            return getValue<T>(Json::nullValue);
        }

        return getValue<T>(jval[first]);
    }

    template <typename T, typename ... Args>
    void set_(Json::Value& jval, T new_val, std::string first) {
        setValue<T>(jval[first], new_val);
    }

    template <typename T, typename ... Args>
    void set_(Json::Value& jval, T new_val, std::string first, Args ... rest) {
        if (!jval.isObject() && !jval.isNull()) {
            throw message_index_error { "invalid message index supplied " };
        }

        if (!jval.isMember(first)) {
            jval[first] = Json::Value {};
        }

        set_<T>(jval[first], new_val, rest...);
    }

    template<typename T>
    T getValue(Json::Value value) const;


    template<typename T>
    void setValue(Json::Value& jval, T new_value);

};

template<typename T>
void DataContainer::setValue(Json::Value& jval, T new_value) {
    jval = new_value;
}

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<std::string> new_value);

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<bool> new_value);

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<int> new_value);

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<float> new_value);

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<double> new_value);

template<>
void DataContainer::setValue<>(Json::Value& jval, std::vector<DataContainer> new_value);

template<>
void DataContainer::setValue<>(Json::Value& jval, DataContainer new_value);

// TODO(ploubser): This becomes the current message imp
// when the message structure changes
class Message : public DataContainer {
  public:
    explicit Message(){}
    explicit Message(std::string msg) : DataContainer(msg) {}
    explicit Message(Json::Value value) : DataContainer(value) {}
};

}  // namespace Agent
}  // namespace Cthun

#endif  // CTHUN_AGENT_SRC_DATA_CONTAINER_H_
