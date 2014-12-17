#ifndef CTHUN_AGENT_SRC_MESSAGE_H_
#define CTHUN_AGENT_SRC_MESSAGE_H_

#include <json/json.h>
#include <cstdarg>
#include <iostream>

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

/// Message abstraction overlaying the raw JSON objects
/// TODO(ploubser): This is an initial implementation with the current message
/// format. Message contents will become more strict when we move to segmented
/// messages. However at the time of CTH-108 this is the starting point for
/// removing the raw json from the agent code.
class Message {
  public:
    Message(){}
    explicit Message(std::string msg);
    std::string toString();

    template <typename T>
    T get(std::string first) {
        return get_<T>(message_root_, first);
    }

    template <typename T, typename ... Args>
    T get(std::string first, Args ... rest) {
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

    template <typename T, typename ... Args>
    T get_(Json::Value jval, std::string first, Args ... rest) {
        if (!(jval.isObject() && jval.isMember(first))) {
            throw message_index_error { "invalid message index supplied " };
        }

        return get_<T>(jval[first], rest...);
    }

    template <typename T>
    T get_(Json::Value jval, std::string first) {
        if (!(jval.isObject() && jval.isMember(first))) {
            throw message_index_error { "invalid message index supplied " };
        }

        return getValue<T>(jval[first]);
    }

    template <typename T, typename ... Args>
    void set_(Json::Value& jval, T new_val, std::string first) {
        jval[first] = new_val;
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
    T getValue(Json::Value value);
};

}  // namespace Agent

#endif  // CTHUN_AGENT_SRC_MESSAGE_H_
