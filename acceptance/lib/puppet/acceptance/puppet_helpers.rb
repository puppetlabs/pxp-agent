require 'scooter'
module Puppet
  module Acceptance
    module PuppetHelpers
      # assumes git cloned and checked out to correct version
      def start_lein_service_from_source_on(host, service_name, conf_file, endpoint, lein_args=nil)
        on(host, "cd /opt/puppet-git-repos/#{service_name}; nohup lein trampoline run #{lein_args} -c #{conf_file} >> /var/log/puppetlabs/#{service_name}-lein.log 2>&1 &")
        curl_retries = host['master-start-curl-retries'] ||
          options['master-start-curl-retries'] || ENV['CURL_RETRIES'] || 99
        # FIXME: argh beaker's curl_with_retries doesn't take curl args and doesn't use --insecure??
        opts = {
          :max_retries => curl_retries,
          :retry_interval => 1
        }
        retry_on(host, "curl -k -m 1 #{endpoint}", opts)
      end

      def force_kill_on(host,service_name)
            on(host, "ps aux | grep #{service_name} | awk {'print $2'} | xargs kill -9", :acceptable_exit_codes => (0..255))
      end

      def stop_puppet_server_on(host, server_from_source = false)
        if host.use_service_scripts?
          step "Ensure puppet is stopped"
          if server_from_source
            force_kill_on(host, '[p]uppet-server')
          else
            # Passenger, in particular, must be shutdown for the cert setup steps to work,
            # but any running puppet master will interfere with webrick starting up and
            # potentially ignore the puppet.conf changes.
            on(host, puppet('resource', 'service', host['puppetservice'], "ensure=stopped"))
          end
        end
      end

      def start_puppet_server_wait_on(host, server_from_source = false)
        url = "https://#{host}:8140"
        if server_from_source
          puppetserver_conf = File.join("#{host['puppetserver-confdir']}", "puppet-server.conf")
          start_lein_service_from_source_on(host, 'puppet-server', puppetserver_conf, url)
        else
          on(host, puppet('resource', 'service', host['puppetservice'], "ensure=running"))
          #sleep_until_puppetserver_started(host)
          #curl_retries = host['master-start-curl-retries'] || options['master-start-curl-retries'] || 99
          #curl_with_retries(host['puppetservice'], host, url, [35, 90], curl_retries)
        end
      end

      def start_deployer_service_wait_on(host, server_from_source = false)
        if server_from_source
          puppet_confdir = host.puppet['confdir']
          conf_file = "#{puppet_confdir}/deployer.conf"
          service_name = 'deployer'
          url = "https://#{host}:9999"
          on(host, "cd /opt/puppet-git-repos/#{service_name}; lein tk -c #{conf_file} >> /var/log/puppetlabs/#{service_name}-lein.log 2>&1 &")
          curl_retries = host['master-start-curl-retries'] || options['master-start-curl-retries'] || 99
          curl_with_retries(" #{service_name} ", host, url, [35, 60], curl_retries)
        else
          service_name = 'pe-xnode-service'
          url = "https://#{host}:8143"
          on(host, puppet('resource', 'service', service_name, "ensure=running"))
        end
      end

    end

    module HTTPHelpers

      def enable_agent_specified_environments ()
        api = Scooter::HttpDispatchers::ConsoleDispatcher.new(dashboard)
        ngroup = api.get_node_group_by_name('Agent-specified environment','agent-specified')
        if ( ! ngroup.keys.include?('rule') )  or ( ! ngroup['rule'][0] = 'or' )
          ngroup['rule']=['or']  # We need to create the rule
        end

        agents.each do |a|
          a_name = on(a, puppet('config print certname')).stdout.rstrip
          ngroup['rule'].push( ['=', 'name', a_name] )
        end

        api.update_node_group(ngroup['id'], ngroup)
      end

      def enable_direct_puppet ()
        api = Scooter::HttpDispatchers::ConsoleDispatcher.new(dashboard)
        node_group = api.get_node_group_by_name('PE Orchestrator')
        api.update_node_group_with_node_group_model(node_group,
                   "classes"=>{"puppet_enterprise::profile::orchestrator"=>{"use_direct_puppet"=>true}})
      end

    end

  end
end
