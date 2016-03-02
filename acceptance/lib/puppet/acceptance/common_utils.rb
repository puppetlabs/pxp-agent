module Puppet
  module Acceptance
    module CommandUtils
      def ruby_command(host)
        "env PATH=\"#{host['privatebindir']}:${PATH}\" ruby"
      end
      module_function :ruby_command
    end

    module CAUtils
      def initialize_ssl
        agents.each do |agent|
          next if agent == master

          ssldir = on(agent, puppet('agent --configprint ssldir')).stdout.chomp
          on(agent, "rm -rf '#{ssldir}'")

          step "Agents: Run agent --test first time to gen CSR"
          on agent, puppet("agent --test --server #{master}"), :acceptable_exit_codes => [1]
        end

        step "Master: sign all certs"
        on master, puppet("cert --sign --all"), :acceptable_exit_codes => [0,24]

        agents.each do |agent|
          next if agent == master

          step "Agents: Run agent --test second time to obtain signed cert"
          on agent, puppet("agent --test --server #{master}"), :acceptable_exit_codes => [0,2]
        end
      end # initialize_ssl

    end # Module CAUtils
  end # Module Acceptance
end # Module Puppet
