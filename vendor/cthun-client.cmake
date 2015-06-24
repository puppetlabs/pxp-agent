include(ExternalProject)

# Add an external project to build horsewhisperer
externalproject_add(
    cthun-client
    PREFIX "${PROJECT_BINARY_DIR}"
    SOURCE_DIR "${VENDOR_DIRECTORY}/cthun-client"
    DOWNLOAD_COMMAND ""
    CMAKE_ARGS "-DCTHUN_CLIENT_LOGGING_PREFIX=${CTHUN_CLIENT_LOGGING_PREFIX}"
    BUILD_IN_SOURCE 0
    BINARY_DIR "${VENDOR_DIRECTORY}/cthun-client/build"
    INSTALL_COMMAND ""
)
externalproject_get_property(cthun-client SOURCE_DIR)
set(CTHUN_CLIENT_INCLUDE_DIRS "${SOURCE_DIR}/lib/inc")
set(CTHUN_CLIENT_LIB "${SOURCE_DIR}/build/lib/libcthun-client.so")
