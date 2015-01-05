find_library(facter_LIBRARY
    NAMES facter)

INCLUDE(FindDependency)
find_dependency(facter DISPLAY "facter" HEADERS "facter/facts/collection.hpp" LIBRARY "facter")


INCLUDE(FeatureSummary)
SET_PACKAGE_PROPERTIES(facter PROPERTIES DESCRIPTION "C++11 implementation of Puppet Facter" URL "https://github.com/puppetlabs/cfacter")
set_package_properties(facter PROPERTIES TYPE OPTIONAL PURPOSE "Enables gathering facts about the system.")
