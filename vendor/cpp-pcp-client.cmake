include(ExternalProject)

# Add an external project to build cpp-pcp-client
externalproject_add(
    cpp-pcp-client
    PREFIX "${PROJECT_BINARY_DIR}"
    SOURCE_DIR "${VENDOR_DIRECTORY}/cpp-pcp-client"
    DOWNLOAD_COMMAND ""
    URL ""
    URL_MD5 ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND "make"
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)
externalproject_get_property(cpp-pcp-client SOURCE_DIR)
set(CPP_PCP_CLIENT_INCLUDE_DIRS "${SOURCE_DIR}/src")
set(CPP_PCP_CLIENT_LIB "${SOURCE_DIR}/lib/libcpp-pcp-client.so")
