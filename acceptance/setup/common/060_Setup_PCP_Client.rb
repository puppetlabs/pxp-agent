require 'pxp-agent/test_helper'
require 'socket'
test_name 'Set up SSL certs for pcp-client to use'

step 'On master create certs for the host running pcp-client' do
  hostname = Socket.gethostname
  on master, puppet("cert generate #{hostname}")
  on(master, puppet('config print ssldir')) do |result|
    puppet_ssldir = result.stdout.chomp
    if !Dir.exists?('tmp/ssl') then Dir.mkdir('tmp/ssl') end
    if !Dir.exists?('tmp/ssl/private_keys') then Dir.mkdir('tmp/ssl/private_keys') end
    if !Dir.exists?('tmp/ssl/public_keys') then Dir.mkdir('tmp/ssl/public_keys') end
    if !Dir.exists?('tmp/ssl/certs') then Dir.mkdir('tmp/ssl/certs') end
    scp_from(master, "#{puppet_ssldir}/private_keys/#{hostname.downcase}.pem", 'tmp/ssl/private_keys')
    scp_from(master, "#{puppet_ssldir}/public_keys/#{hostname.downcase}.pem", 'tmp/ssl/public_keys')
    scp_from(master, "#{puppet_ssldir}/certs/#{hostname.downcase}.pem", 'tmp/ssl/certs')
    scp_from(master, "#{puppet_ssldir}/certs/ca.pem", 'tmp/ssl/certs')
  end
end
