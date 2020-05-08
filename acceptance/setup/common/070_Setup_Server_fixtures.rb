require 'puppet/acceptance/environment_utils.rb'

test_name 'Set up puppetserver fixtures' do

  step 'stop puppetserver' do
    on master, puppet('resource service puppetserver ensure=stopped')
  end

  PUPPETSERVER_CONFIG_FILE = '/etc/puppetlabs/puppetserver/conf.d/webserver.conf'

  fixtures = File.absolute_path('files')
  test_env = 'bolt'
  env_path = ''

  extend Puppet::Acceptance::EnvironmentUtils
  step "Copy #{test_env} environment fixtures to server" do
    fixture_env = File.join(fixtures, 'environments', test_env)
    filename = 'testing_file.txt'
    env_path = File.join(environmentpath, test_env)
    on(master, "mkdir -p #{env_path}")
    rsync_to(master, fixture_env, env_path)
  end

  step "Setup static file mount on puppetserver for #{test_env} environment" do
    hocon_file_edit_in_place_on(master, PUPPETSERVER_CONFIG_FILE) do |host, doc|
      doc.set_config_value("webserver.static-content", Hocon::ConfigValueFactory.from_any_ref([{
        "path" => "/#{test_env}",
        "resource" => env_path
      }]))
    end
  end

  step 'start puppetserver' do
    on master, puppet('resource service puppetserver ensure=running')
  end
end