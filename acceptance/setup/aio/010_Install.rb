require 'puppet/acceptance/common_utils'
require 'beaker/dsl/install_utils'
extend Beaker::DSL::InstallUtils

test_name "Install Packages"

def install_from_local(hosts, package)
  hosts.each do |agent|
    tmpdir = agent.tmpdir('puppet_agent')

    step "Copy package to #{agent}" do
      scp_to(agent, package, tmpdir)
    end

   # Move the openssl libs package to a newer version on redhat platforms
   use_system_openssl = ENV['USE_SYSTEM_OPENSSL']

   if use_system_openssl && agent[:platform].match(/(?:el-7|redhat-7)/)
     rhel7_openssl_version = ENV['RHEL7_OPENSSL_VERSION']
     if rhel7_openssl_version.to_s.empty?
       # Fallback to some default is none is provided
       rhel7_openssl_version = "openssl-1.0.1e-51.el7_2.4.x86_64"
     end
     on(agent, "yum -y install " +  rhel7_openssl_version)
   else
     step "Skipping upgrade of openssl package... (" + agent[:platform] + ")"
   end

    step "Install package on #{agent}" do
      agent.install_package("#{tmpdir}/#{File.basename(package)}")
    end
  end
end

step "Install puppet-agent..." do
  if ENV['PACKAGE']
    step "from local package #{ENV['PACKAGE']}" do
      install_from_local(agents, ENV['PACKAGE'])
    end
  else
    opts = {
      :puppet_collection    => 'PC1',
      :puppet_agent_sha     => ENV['SHA'],
      :puppet_agent_version => ENV['SUITE_VERSION'] || ENV['SHA']
    }
    agents.each do |agent|
      next if agent == master # Avoid SERVER-528

      use_system_openssl = ENV['USE_SYSTEM_OPENSSL']
      if use_system_openssl && agent[:platform].match(/(?:el-7|redhat-7)/)
        rhel7_openssl_version = ENV['RHEL7_OPENSSL_VERSION']
        if rhel7_openssl_version.to_s.empty?
          # Fallback to some default is none is provided
          rhel7_openssl_version = "openssl-1.0.1e-51.el7_2.4.x86_64"
        end
        on(agent, "yum -y install " +  rhel7_openssl_version)
      else
        step "Skipping upgrade of openssl package... (" + agent[:platform] + ")"
      end

      install_puppet_agent_dev_repo_on(agent, opts)
    end
  end
end

step "Install puppetserver..." do
  if master[:hypervisor] == 'ec2' && master[:platform].match(/(?:el|centos|oracle|redhat|scientific)/)
    variant, version = master['platform'].to_array
    server_num = ENV['SERVER_VERSION'].to_i
    # nil or 'latest' will convert to 0
    if server_num > 0 && server_num < 5
      logger.info "EC2 master found: Installing public PC1 repo to satisfy puppetserver dependency."
      on(master, "rpm -Uvh https://yum.puppetlabs.com/puppetlabs-release-pc1-el-#{version}.noarch.rpm")
    else
      logger.info "EC2 master found: Installing public puppet5 repo to satisfy puppetserver dependency."
      on(master, "rpm -Uvh https://yum.puppetlabs.com/puppet5-release-el-#{version}.noarch.rpm")
    end
  else
    if ENV['SERVER_VERSION'].nil? || ENV['SERVER_VERSION'] == 'latest'
      server_version = 'latest'
      server_download_url = "http://nightlies.puppet.com"
    else
      server_version = ENV['SERVER_VERSION']
      server_download_url = "http://builds.delivery.puppetlabs.net"
    end
    install_puppetlabs_dev_repo(master, 'puppetserver', server_version, nil, :dev_builds_url => server_download_url)

    # Bump version of openssl on rhel7 platforms
    use_system_openssl = ENV['USE_SYSTEM_OPENSSL']
    if use_system_openssl && master[:platform].match(/(?:el-7|redhat-7)/)
      rhel7_openssl_version = ENV['RHEL7_OPENSSL_VERSION']
      if rhel7_openssl_version.to_s.empty?
        # Fallback to some default is none is provided
        rhel7_openssl_version = "openssl-1.0.1e-51.el7_2.4.x86_64"
      end
      on(master, "yum -y install " +  rhel7_openssl_version)
    else
      step "Skipping upgrade of openssl package... (" + master[:platform] + ")"
    end

    install_puppetlabs_dev_repo(master, 'puppet-agent', ENV['SHA'])
  end
  master.install_package('puppetserver')
end

step 'Make sure install is sane' do
  # beaker has already added puppet and ruby to PATH in ~/.ssh/environment
  agents.each do |agent|
    on agent, puppet('--version')
    ruby = Puppet::Acceptance::CommandUtils.ruby_command(agent)
    on agent, "#{ruby} --version"
  end
end


step "Enable FIPS on agent hosts..." do

 # There might be a better way to do some things below but till then...
 # The TODO's need to be followed up

  agents.each do |agent|
    next if agent == master # Only on agents.

    # Do this only on rhel7, rhel6, f24, f25
    use_system_openssl = ENV['USE_SYSTEM_OPENSSL']
    if use_system_openssl
      # Other platforms to come...
      next if !agent[:platform].match(/(?:el-7|redhat-7)/)

      # Step 1: Disable prelinking
      # TODO:: Handle cases where the /etc/sysconfig/prelink might exist
      on(agent, 'echo "PRELINKING=no" > /etc/sysconfig/prelink')
      # TODO: prelink is commented till we figure how to attempt executing
      # it so any failures do not cause this thing to exit
      # on(agent, "prelink -u -a")
 
      # Step 2: Install dracut-fips, dracut-fips-aesni
      on(agent, "yum -y install dracut-fips")
      on(agent, "yum -y install dracut-fips-aesni")

      # TODO:: Backup existing kernel image in case anything goes south..
      # Step 3: Run dracut to update system for fips. 
      on(agent, "dracut -v -f")


      # TODO:: Steps 4 and 5 need to be adjusted for rhel6, f24, f25
      # as the method of enabling FIPS are different on those platforms. 
      # Below works on rhel7
      
      # Step 4: Find out the device details (BLOCK ID) of boot partition
      # append enable fips and boot block id to kernel command line in grub file
      
      on(agent, 'boot_blkid=$(blkid `df /boot | grep "/dev" | awk \'BEGIN{ FS=" "}; {print $1}\'` | awk \'BEGIN{ FS=" "}; {print $2}\' | sed \'s/"//g\');\
                 fips_bootblk="fips=1 boot="$boot_blkid;\
                 grub_linux_cmdline=`grep -e "^GRUB_CMDLINE_LINUX" /etc/default/grub | sed "s/\"$/ $fips_bootblk\"/"`;\
                 grep -v GRUB_CMDLINE_LINUX /etc/default/grub > /etc/default/grub.bak;\
                 /bin/cp -rf /etc/default/grub.bak /etc/default/grub;\
                 sed -i "/GRUB_DISABLE_RECOVERY/i $grub_linux_cmdline" /etc/default/grub')


      # Step 5: Run grub2 mkconfig to make the changes take effect.
      on(agent, 'grub2-mkconfig -o /boot/grub2/grub.cfg')

      # Reboot is needed for the system to start in FIPS mode
      agent.reboot
      timeout = 30
      begin
        Timeout.timeout(timeout) do
          until agent.up?
            sleep 1
          end
        end
      rescue Timeout::Error => e
        raise "Agent did not come back up within #{timeout} seconds."
      end
      
      # TODO:
      # Ensure that agent is actually in fips mode by examining the contents of
      # /proc/sys/crypto/fips_enabled and that it is 1

      # Switch to using sha256 to work in fips mode.
      on agent, puppet('config set digest_algorithm sha256')
      
    end
  end
end
