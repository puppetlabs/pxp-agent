include(ExternalProject)

# Add an external project to build horsewhisperer
externalproject_add(
    cthun-client
    PREFIX "${PROJECT_BINARY_DIR}"
    SOURCE_DIR "${VENDOR_DIRECTORY}/cthun-client"
    DOWNLOAD_COMMAND ""
    URL ""
    URL_MD5 ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND "make"
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)
externalproject_get_property(cthun-client SOURCE_DIR)
set(CTHUN_CLIENT_INCLUDE_DIRS "${SOURCE_DIR}/src")
set(CTHUN_CLIENT_LIB "${SOURCE_DIR}/lib/libcthun-client.so")
