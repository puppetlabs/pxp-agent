INCLUDE(FindDependency)
find_dependency(BLKID DISPLAY "facter" HEADERS "facter/facts/collection.hpp" LIBRARY "facter")


INCLUDE(FeatureSummary)
SET_PACKAGE_PROPERTIES(Cfacter PROPERTIES DESCRIPTION "C++11 implementation of Puppet Facter" URL "https://github.com/puppetlabs/cfacter")
set_package_properties(Cfacter PROPERTIES TYPE OPTIONAL PURPOSE "Enables gathering facts about the system.")
