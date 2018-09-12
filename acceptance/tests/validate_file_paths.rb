test_name 'C93828 - pxp-agent - File paths from spec'

# verify that the installed file paths match those of the
# puppet specification.
# https://github.com/puppetlabs/puppet-specifications/blob/master/file_paths.md
#

# {{{ helper methods
def config_options(host)
  platform = host[:platform]
  ruby_arch = host[:ruby_arch]
  type =  @options[:type]

  case platform
  when /windows/
    if platform =~ /2003/
      common_app_data = 'C:/Documents and Settings/All Users/Application Data'
    else
      common_app_data = 'C:/ProgramData'
    end
    if ruby_arch == 'x64'
      ruby_arch = /-64/
    else
      ruby_arch = /-32/
    end
    if platform =~ ruby_arch
      install_root = 'C:/Program Files/Puppet Labs/Puppet'
    else
      install_root = 'C:/Program Files (x86)/Puppet Labs/Puppet'
    end

    exdir = "#{install_root}/pxp-agent"
    pupdir = "#{install_root}/puppet"

    binfile = "#{pupdir}/bin/pxp-agent.exe"
    moduledir = "#{exdir}/modules"
    puppetlabs_data = "#{common_app_data}/PuppetLabs"
    confdir = "#{puppetlabs_data}/pxp-agent/etc"
    module_confdir = "#{confdir}/modules"
    vardir = "#{puppetlabs_data}/pxp-agent/var"
    logdir = "#{vardir}/log"
    spooldir = "#{vardir}/spool"
    rundir = "#{vardir}/run"
    nssm =  "#{pupdir}/bin/nssm.exe"

  else
    bindir = '/opt/puppetlabs/puppet/bin'
    binfile = "#{bindir}/pxp-agent"
    confdir = '/etc/puppetlabs/pxp-agent'
    module_confdir = '/etc/puppetlabs/pxp-agent/modules'
    pxpdir = '/opt/puppetlabs/pxp-agent'
    moduledir = "#{pxpdir}/modules"
    spooldir = "#{pxpdir}/spool"
    logdir = '/var/log/puppetlabs/pxp-agent'
    rundir = '/var/run/puppetlabs'
  end

  [
    # bin
    {:name => :binfile,         :expected => binfile,                     :installed => :file},
    {:name => :nssm,            :expected => nssm,                        :installed => :file},

    # config
    {:name => :confdir,         :expected => confdir,                     :installed => :dir},
    #{:name => :config,          :expected => "#{confdir}/pxp-agent.conf", :installed => :file},

    # modules
    {:name => :moduledir,       :expected => moduledir,                   :installed => :dir},
    {:name => :module_confdir,  :expected => module_confdir,              :installed => :dir},
    {:name => :puppetmodule,    :expected => "#{moduledir}/pxp-module-puppet", :installed => :file},
    #{:name => :puppetmodule_conf, :expected => "#{module_confdir}/pxp-module-puppet.conf", :installed => :file},

    # logdir/rundir
    {:name => :logdir,          :expected => logdir,                      :installed => :dir},
    {:name => :spooldir,        :expected => spooldir,                    :installed => :dir},
    {:name => :rundir,          :expected => rundir,                      :installed => :dir},
    {:name => :vardir,          :expected => vardir,                      :installed => :dir},
    #{:name => :pidfile,         :expected => "#{rundir}/pxp-agent.pid"},
  ]
end

# }}}

step 'test pxp config paths exist'
agents.each do |agent|
  config_options(agent).each do |config_option|
    path = config_option[:expected]
    if path.to_s == '' then
      next
    end
    case config_option[:installed]
    when :dir
      on(agent, "test -d \"#{path}\"", :acceptable_exit_codes => [0, 1])  do |result|
        unless result.exit_code == 0
          fail_test("Failed to find expected directory '#{path}' on platform '#{agent[:platform]}' agent '#{agent}'")
        end
      end
    when :file
      on(agent, "test -f \"#{path}\"", :acceptable_exit_codes => [0, 1])  do |result|
        unless result.exit_code == 0
          fail_test("Failed to find expected file '#{path}' on platform '#{agent[:platform]}' agent '#{agent}'")
        end
      end
    end
  end
end
