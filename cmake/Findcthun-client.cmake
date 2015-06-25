find_library(cthun-client_LIBRARY
             NAMES cthun-client)

include(FindDependency)
find_dependency(cthun-client 
    DISPLAY "cthun-client"
    HEADERS "cthun-client/connector/connection.hpp"
    LIBRARY "cthun-client")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(cthun-client DEFAULT_MSG cthun-client_LIBRARY cthun-client_INCLUDE_DIR)
