# Acceptance Tests

Acceptance tests use https://github.com/puppetlabs/pcp-broker#HEAD by default.

They can be configured to test against other versions of pcp-broker by setting environment variables

    PCP_BROKER_FORK=puppetlabs
    PCP_BROKER_REF=0.8.4

To test using PCP v2 - when using a broker that supports it - use

    PCP_VERSION=2
