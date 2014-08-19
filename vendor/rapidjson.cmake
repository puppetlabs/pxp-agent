cmake_minimum_required(VERSION 2.8.12)
include(ExternalProject)

# Add an external project to build rapidjson
externalproject_add(
    rapidjson
    PREFIX "${PROJECT_BINARY_DIR}"
    URL "file://${VENDOR_DIRECTORY}/rapidjson-c0a7922.zip"
    URL_MD5 "d03793bf2158a1c5aae5b7abbb03435f"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)

# Set some useful variables based on the source directory
externalproject_get_property(rapidjson SOURCE_DIR)
set(RAPIDJSON_INCLUDE_DIRS "${SOURCE_DIR}/include")
