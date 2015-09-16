cmake_minimum_required(VERSION 2.8.12)
include(ExternalProject)

# Add an external project to build boost-process
externalproject_add(
    boost-process
    PREFIX "${PROJECT_BINARY_DIR}"
    URL "file://${VENDOR_DIRECTORY}/boost-process.zip"
    URL_MD5 "1040619a5c552e684fd445fe29fb13c7"
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

# Set some useful variables based on the source directory
externalproject_get_property(boost-process SOURCE_DIR)
set(Boost_Process_INCLUDE_DIRS "${SOURCE_DIR}")
