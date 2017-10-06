require 'pxp-agent/test_helper.rb'
require 'yaml'

test_name  'Run puppet agent as non-root' do

  agents.each do |agent|
    platform = agent.platform
    skip_test "Test is not compatible with #{platform}" if platform =~ /windows/

    step 'create non-root user on all nodes' do
      on(agent, puppet("resource user foo ensure=present managehome=true"))

      #TODO fix this only one agent ???
      @user_home_dir = YAML.load(on(agent, puppet("resource user foo --to_yaml")).stdout)['user']['foo']['home']
      @user_puppetlabs_dir = "#{@user_home_dir}/.puppetlabs"
      @user_puppet_dir = "#{@user_puppetlabs_dir}/etc/puppet"
      @user_pxp_dir = "#{@user_puppetlabs_dir}/etc/pxp-agent"
      on(agent, "mkdir -p #{@user_puppet_dir}")
      on(agent, "mkdir -p #{@user_pxp_dir}")
    end

    teardown do
      get_process_pids(agent, 'pxp-agent').each do |pid|
        on(agent, "kill -9 #{pid}", :accept_all_exit_codes => true)
      end
      on(agent, puppet("resource user foo ensure=absent managehome=true"))
    end

    step 'Ensure pxp-agent is stopped' do
      on(agent, puppet("resource service pxp-agent ensure=stopped enable=false"))
    end

    step 'Copy certs and keys to new users home directory' do
      puppet_ssldir = on(agent, puppet('config print ssldir')).stdout.chomp
      #ssl-key ssl-ca-cert ssl-cert
      on(agent, "cp -R #{puppet_ssldir} #{@user_puppet_dir}")
    end

    step 'Attempt start of pxp-agent as non-root user' do
      on( agent, puppet("resource service puppet ensure=running enable=true"))
      on( agent, puppet('resource service pxp-agent ensure=stopped'))

      ssl_config = {
          :ssl_key        => "#{@user_puppet_dir}/ssl/private_keys/#{agent}.pem",
          :ssl_ca_cert    => "#{@user_puppet_dir}/ssl/certs/ca.pem",
          :ssl_cert       => "#{@user_puppet_dir}/ssl/certs/#{agent}.pem"
      }

      create_remote_file(agent, "#{@user_pxp_dir}/pxp-agent.conf", pxp_config_hocon(master, agent, ssl_config).to_s)
      on(agent, "chown foo -R #{@user_puppetlabs_dir}")
      on(agent, "su foo -c \"/opt/puppetlabs/puppet/bin/pxp-agent\"", :accept_all_exit_codes => true) do |result|
        assert_equal(0, result.exit_code, "The expected exit code was not observed \n #{result.output}")
      end

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "At the start of the test, #{agent} (with PCP identity pcp://#{agent}/agent ) should be associated with pcp-broker")
    end
  end
end
