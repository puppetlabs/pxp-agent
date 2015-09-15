include(FindDependency)
find_dependency(cpp-pcp-client
    DISPLAY "cpp-pcp-client"
    HEADERS "cpp-pcp-client/connector/connection.hpp"
    LIBRARIES "cpp-pcp-client"
    REQUIRED)
