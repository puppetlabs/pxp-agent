# Acceptance Tests

Acceptance tests use https://github.com/puppetlabs/pcp-broker#HEAD by default.

They can be configured to test against other versions of pcp-broker by setting environment variables

    PCP_BROKER_FORK=puppetlabs
    PCP_BROKER_REF=0.8.4

Default settings use PCP v2. To test using PCP v1 set

    PCP_VERSION=1

To run acceptance tests, a build of puppet-agent is required. An example of how to run them is

    rake ci:test:aio SHA=1.8.2 TEST_TARGET=centos7-64a

You can run `rake ci:test:aio` to get more detailed help about how to configure testing.

Additionally, PACKAGE can be set to install a local package built via [puppet-agent](https://github.com/puppetlabs/puppet-agent)
on agents during testing; SHA is still required for the version to install on the master. For example

    rake ci:test:aio SHA=1.8.2 TEST_TARGET=centos7-64a PACKAGE=/path/to/puppet-agent/output/el/7/PC1/x86_64/puppet-agent-5.0.1.190.g4263fa6d-1.el7.x86_64.rpm
