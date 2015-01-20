include(ExternalProject)

# Add an external project to build inih
# url: https://inih.googlecode.com/files/inih_r29.zip
externalproject_add(
    inih
    PREFIX "${PROJECT_BINARY_DIR}"
    URL "file://${VENDOR_DIRECTORY}/inih_r29.zip"
    URL_MD5 "ebdbf3c5a7d316e17cb5b08e60c37673"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
)

# Set some useful variables based on the source directory
externalproject_get_property(inih SOURCE_DIR)

set(INIH_SOURCE_DIR "${SOURCE_DIR}")
set(INIH_INCLUDE_DIRS "${SOURCE_DIR}/cpp" "${SOURCE_DIR}")
