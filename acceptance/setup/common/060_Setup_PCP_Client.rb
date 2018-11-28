require 'pxp-agent/test_helper'
require 'socket'
require 'fileutils'

test_name 'Set up SSL certs for pcp-client to use'

step 'On master create certs for the host running pcp-client' do
  master_fqdn = on(master, 'facter fqdn').stdout.strip
  on master, puppet("config set server #{master_fqdn} --section main")

  hostname = Socket.gethostname.downcase
  on master, "puppetserver ca generate --certname #{hostname}"
  on(master, puppet('config print ssldir')) do |result|
    puppet_ssldir = result.stdout.chomp
    FileUtils.mkdir_p('tmp/ssl/private_keys')
    FileUtils.mkdir_p('tmp/ssl/public_keys')
    FileUtils.mkdir_p('tmp/ssl/certs')
    scp_from(master, "#{puppet_ssldir}/private_keys/#{hostname}.pem", 'tmp/ssl/private_keys')
    scp_from(master, "#{puppet_ssldir}/public_keys/#{hostname}.pem", 'tmp/ssl/public_keys')
    scp_from(master, "#{puppet_ssldir}/certs/#{hostname}.pem", 'tmp/ssl/certs')
    scp_from(master, "#{puppet_ssldir}/certs/ca.pem", 'tmp/ssl/certs')
  end
end
