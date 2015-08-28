find_library(cpp-pcp-client_LIBRARY
             NAMES cpp-pcp-client)

include(FindDependency)
find_dependency(cpp-pcp-client
    DISPLAY "cpp-pcp-client"
    HEADERS "cpp-pcp-client/connector/connection.hpp"
    LIBRARY "cpp-pcp-client")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cpp-pcp-client DEFAULT_MSG cpp-pcp-client_LIBRARY cpp-pcp-client_INCLUDE_DIR)
