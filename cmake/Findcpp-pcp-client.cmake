include(FindDependency)
find_dependency(cpp-pcp-client
    DISPLAY "cpp-pcp-client"
    HEADERS "cpp-pcp-client/connector/connection.hpp"
    LIBRARIES "libcpp-pcp-client.so" "cpp-pcp-client"
    REQUIRED)
