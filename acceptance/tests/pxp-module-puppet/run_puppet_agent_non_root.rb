require 'pxp-agent/test_helper.rb'
require 'yaml'

test_name  'Run puppet agent as non-root' do

  agents.each do |agent|
    platform = agent.platform
    skip_test "Test is not compatible with #{platform}" if platform =~ /windows/

    step 'create non-root user on all nodes' do
      @user_name = platform =~ /osx-10.14/ ? 'osx' : 'foo'
      @group_name = 'foobar'
      if platform =~ /solaris/
        @user_home_dir = "/export/home/#{@user_name}"
      elsif platform =~ /osx/
        @user_home_dir = "/Users/#{@user_name}"
      else
        @user_home_dir = "/home/#{@user_name}"
      end
      @user_puppetlabs_dir = "#{@user_home_dir}/.puppetlabs"
      @user_puppet_dir = "#{@user_puppetlabs_dir}/etc/puppet"
      @user_pxp_dir = "#{@user_puppetlabs_dir}/etc/pxp-agent"
      on(agent, "mkdir -p #{@user_puppet_dir}")
      on(agent, "mkdir -p #{@user_pxp_dir}")
      on(agent, puppet("resource group #{@group_name} ensure=present"))
      if platform =~ /eos/
        #arista
        on(agent, "useradd #{@user_name} -p p@ssw0rd")
      else
        on(agent, puppet("resource user #{@user_name} ensure=present home=#{@user_home_dir} groups=#{@group_name}"))
      end
    end

    teardown do
      get_process_pids(agent, 'pxp-agent').each do |pid|
        on(agent, "kill -9 #{pid}", :accept_all_exit_codes => true)
      end
      next if agent.platform =~ /osx-10.14/
      on(agent, puppet("resource user #{@user_name} ensure=absent"))
      on(agent, "rm -rf #{@user_home_dir}")
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
      ssl_config = {
          :ssl_key        => "#{@user_puppet_dir}/ssl/private_keys/#{agent}.pem",
          :ssl_ca_cert    => "#{@user_puppet_dir}/ssl/certs/ca.pem",
          :ssl_cert       => "#{@user_puppet_dir}/ssl/certs/#{agent}.pem"
      }

      create_remote_file(agent, "#{@user_pxp_dir}/pxp-agent.conf", pxp_config_hocon(master, agent, ssl_config).to_s)
      on(agent, "chown -R #{@user_name}:#{@group_name} #{@user_home_dir}")

      if platform =~ /solaris|aix|eos/
        command = "cd #{@user_home_dir} && HOME=#{@user_home_dir} su #{@user_name} -c \"/opt/puppetlabs/puppet/bin/pxp-agent\""
      else
        command = "su -l #{@user_name} -c \"/opt/puppetlabs/puppet/bin/pxp-agent\""
      end

      on(agent, command, :accept_all_exit_codes => true) do |result|
        assert_equal(0, result.exit_code, "The expected exit code was not observed \n #{result.output}")
      end

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "At the start of the test, #{agent} (with PCP identity pcp://#{agent}/agent ) should be associated with pcp-broker")
    end
  end
end
