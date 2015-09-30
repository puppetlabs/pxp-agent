require 'puppet/acceptance/puppet_helpers'
extend Puppet::Acceptance::PuppetHelpers

module Puppet
  module Acceptance
    module CAUtils

      def initialize_ssl
        step 'Agent: Run agent --test first time to gen CSR'
        # allow exit code of 0 here in case certs are already signed
        on agents, puppet("agent --test --server #{master} --environment provision"), :acceptable_exit_codes => [1]
        # TODO: skip next steps if above exited 0

        # Sign all waiting certs
        step 'Master: sign all certs'
        on master, puppet('cert --sign --all'), :acceptable_exit_codes => [0,24]

        step 'Agents: Run agent --test second time with signed cert'
        on agents, puppet("agent --test --server #{master} --environment provision"), :acceptable_exit_codes => [0,2]
      end

    end
  end
end
