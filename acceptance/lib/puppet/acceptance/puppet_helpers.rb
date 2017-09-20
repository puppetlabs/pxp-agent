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
    end
  end
end
