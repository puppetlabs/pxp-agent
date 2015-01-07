cmake_minimum_required(VERSION 2.8.12)
include(ExternalProject)


# zip sourced from https://github.com/miloyip/rapidjson/archive/master.zip

# Add an external project to unpack rapidjson
externalproject_add(
    rapidjson
    PREFIX "${PROJECT_BINARY_DIR}"
    URL "file://${VENDOR_DIRECTORY}/rapidjson-master.zip"
    URL_MD5 "a6ed2099b3c7fdbd9a00880bac7b11a6"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)

# Set some useful variables based on the source directory
externalproject_get_property(rapidjson SOURCE_DIR)
set(RAPIDJSON_INCLUDE_DIRS "${SOURCE_DIR}/include")
