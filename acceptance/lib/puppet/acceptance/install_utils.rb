require 'open-uri'
require 'open3'
require 'uri'
require 'puppet/acceptance/common_utils'

module Puppet
  module Acceptance
    module InstallUtils
      PLATFORM_PATTERNS = {
        :redhat        => /fedora|el|centos/,
        :debian8       => /debian-?8/,
        :debian        => /debian|ubuntu/,
        :sles          => /sles/,
        :solaris_10    => /solaris-10/,
        :solaris_11    => /solaris-11/,
        :windows       => /windows/,
      }.freeze

      # Installs packages on the hosts.
      #
      # @param hosts [Array<Host>] Array of hosts to install packages to.
      # @param package_hash [Hash{Symbol=>Array<String,Array<String,String>>}]
      #   Keys should be a symbol for a platform in PLATFORM_PATTERNS.  Values
      #   should be an array of package names to install, or of two element
      #   arrays where a[0] is the command we expect to find on the platform
      #   and a[1] is the package name (when they are different).
      # @param options [Hash{Symbol=>Boolean}]
      # @option options [Boolean] :check_if_exists First check to see if
      #   command is present before installing package.  (Default false)
      # @return true
      def install_packages_on(hosts, package_hash, options = {})
        check_if_exists = options[:check_if_exists]
        hosts = [hosts] unless hosts.kind_of?(Array)
        hosts.each do |host|
          package_hash.each do |platform_key,package_list|
            if pattern = PLATFORM_PATTERNS[platform_key]
              if pattern.match(host['platform'])
                package_list.each do |cmd_pkg|
                  if cmd_pkg.kind_of?(Array)
                    command, package = cmd_pkg
                  else
                    command = package = cmd_pkg
                  end
                  if !check_if_exists || !host.check_for_package(command)
                    host.logger.notify("Installing #{package}")
                    additional_switches = '--allow-unauthenticated' if platform_key == :debian
                    host.install_package(package, additional_switches)
                  end
                end
              end
            else
              raise("Unknown platform '#{platform_key}' in package_hash")
            end
          end
        end
        return true
      end

      def stop_firewall_on(host)
        case host['platform']
        when /debian/
          on host, 'iptables -F'
        when /fedora|el-7/
          on host, puppet('resource', 'service', 'firewalld', 'ensure=stopped')
          on host, puppet('resource', 'service', 'iptables', 'ensure=stopped')
        when /el|centos/
          on host, puppet('resource', 'service', 'firewalld', 'ensure=stopped')
          on host, puppet('resource', 'service', 'iptables', 'ensure=stopped')
        when /ubuntu/
          on host, puppet('resource', 'service', 'ufw', 'ensure=stopped')
        else
          logger.notify("Not sure how to clear firewall on #{host['platform']}")
        end
      end

      def install_repos_on(host, project, sha, repo_configs_dir)
        platform = host['platform'].with_version_codename
        tld     = sha == 'nightly' ? 'nightlies.puppetlabs.com' : 'builds.puppetlabs.lan'
        project = sha == 'nightly' ? project + '-latest'        :  project
        sha     = sha == 'nightly' ? nil                        :  sha

        case platform
        when /^(fedora|el|centos)-(\d+)-(.+)$/
          variant = (($1 == 'centos') ? 'el' : $1)
          fedora_prefix = ((variant == 'fedora') ? 'f' : '')
          version = $2
          arch = $3

          # install repo so deps can be fulfilled when required
          # todo: this should be moved outside of here so we don't reinstall with each repos installed
          puppetlabs_repo_url = "http://yum.puppetlabs.com/puppetlabs-release-%s-%s.noarch.rpm" % [variant, version]
          on host, "rpm -Uvh --force #{puppetlabs_repo_url}"

          # grab the repo for SHA of project
          repo_filename = "pl-%s%s%s-%s-%s%s-%s.repo" % [
            project,
            sha ? '-' + sha : '',
            project =~ /xnode-service/ ? '-repos-pe' : '',
            variant,
            fedora_prefix,
            version,
            arch
          ]
          repo_url = "http://%s/%s/%s/repo_configs/rpm/%s" % [tld, project, sha, repo_filename]

          on host, "curl -o /etc/yum.repos.d/#{repo_filename} #{repo_url}"
        when /^(debian|ubuntu)-([^-]+)-(.+)$/
          variant = $1
          version = $2
          arch = $3

          # install repo so deps can be fulfilled when required
          # todo: this should be moved outside of here so we don't reinstall with each repos installed
          deb_filename = "puppetlabs-release-#{version}.deb"
          puppetlabs_repo_url = "http://apt.puppetlabs.com/#{deb_filename}"
          puppetlabs_repo_path = "/tmp/#{deb_filename}"
          on host, "curl -o #{puppetlabs_repo_path} #{puppetlabs_repo_url}"
          on host, "dpkg -i --force-all #{puppetlabs_repo_path}"

          list_filename = "pl-%s%s-%s.list" % [
            project,
            sha ? '-' + sha : '',
            version
          ]
          list_url = "http://%s/%s/%s/repo_configs/deb/%s" % [tld, project, sha, list_filename]

          on host, "curl -o /etc/apt/sources.list.d/#{list_filename} #{list_url}"
          on host, 'apt-get update'
        else
          host.logger.notify("No repository installation step for #{platform} yet...")
        end
      end

      # Configures gem sources on hosts to use a mirror, if specified
      # This is a duplicate of the Gemfile logic.
      def configure_gem_mirror(hosts)
        hosts = [hosts] unless hosts.kind_of?(Array)
        gem_source = ENV['GEM_SOURCE'] || 'https://rubygems.org'

        hosts.each do |host|
          gem = Puppet::Acceptance::CommandUtils.gem_command(host)
          on host, "#{gem} source --clear-all"
          on host, "#{gem} source --add #{gem_source}"
        end
      end
    end
  end
end
