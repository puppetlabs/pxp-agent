require 'pxp-agent/bolt_pxp_module_helper.rb'
require 'pxp-agent/config_helper.rb'
require 'puppet/acceptance/environment_utils.rb'

suts = agents.reject { |host| host['roles'].include?('master') }

def clean_files(agent, target_file = nil)
  tasks_cache = get_tasks_cache(agent)
  assert_match(/ensure\s+=> 'absent'/, on(agent, puppet("resource file #{tasks_cache}/apply_ruby_shim ensure=absent force=true")).stdout)
  if target_file
    assert_match(/ensure\s+=> 'absent'/,
                 on(agent, puppet("resource file #{target_file} ensure=absent force=true")).stdout)
  end
end

def apply_catalog_entry(catalog, apply_options: { noop: false })
  catalog.merge({ apply_options: apply_options})
end

# Apply a notify resource. Even though this does not affect any change on the target, it is platform independent
# and does not rely on any plugin libs which makes it a good smoke test across any target.
def cross_platform_catalog(certname, environment = 'production')
  {
    "catalog" => {
      "tags" => [
        "settings"
      ],
      "name" => certname,
      "version" => 1581636379,
      "code_id" => nil,
      "catalog_uuid" => "5a4372c6-253f-46df-be99-3c40c9922423",
      "catalog_format" => 1,
      "environment" => environment,
      "resources" => [
        {
          "type" => "Stage",
          "title" => "main",
          "tags" => [
            "stage",
            "class"
          ],
          "exported" => false,
          "parameters" => {
            "name" => "main"
          }
        },
        {
          "type" => "Class",
          "title" => "Settings",
          "tags" => [
            "class",
            "settings"
          ],
          "exported" => false
        },
        {
          "type" => "Class",
          "title" => "main",
          "tags" => [
            "class"
          ],
          "exported" => false,
          "parameters" => {
            "name" => "main"
          }
        },
        {
          "type" => "Notify",
          "title" => "hello world",
          "tags" => [
            "notify",
            "class"
          ],
          "line" => 1,
          "exported" => false
        }
      ],
      "edges" => [
        {
          "source" => "Stage[main]",
          "target" => "Class[Settings]"
        },
        {
          "source" => "Stage[main]",
          "target" => "Class[main]"
        },
        {
          "source" => "Class[main]",
          "target" => "Notify[hello world]"
        }
      ],
      "classes" => [
        "settings"
      ]
    }
  }
end

def plugin_dependend_catalog(certname, environment = 'production')
  {
    "catalog" => {
      "tags" => [
        "settings"
      ],
      "name" => certname,
      "version" => "baleful-library-apply-7c48f834645",
      "code_id" => nil,
      "catalog_uuid" => "257b0266-6807-4c8f-838a-2e15861bce00",
      "catalog_format" => 1,
      "environment" => environment,
      "resources" => [
        {
          "type" => "Stage",
          "title" => "main",
          "tags" => [
            "stage",
            "class"
          ],
          "exported" => false,
          "parameters" => {
            "name" => "main"
          }
        },
        {
          "type" => "Class",
          "title" => "Settings",
          "tags" => [
            "class",
            "settings"
          ],
          "exported" => false
        },
        {
          "type" => "Class",
          "title" => "main",
          "tags" => [
            "class"
          ],
          "exported" => false,
          "parameters" => {
            "name" => "main"
          }
        },
        {
          "type" => "Warn",
          "title" => "Hello\\!",
          "tags" => [
            "warn",
            "class"
          ],
          "line" => 1,
          "exported" => false
        }
      ],
      "edges" => [
        {
          "source" => "Stage[main]",
          "target" => "Class[Settings]"
        },
        {
          "source" => "Stage[main]",
          "target" => "Class[main]"
        },
        {
          "source" => "Class[main]",
          "target" => "Warn[Hello\\!]"
        }
      ],
      "classes" => [
        "settings"
      ]
    }
  }
end

def file_dependent_catalog(certname, file_path, environment = 'production')
  {
    "catalog": {
      "tags": [
        "settings"
      ],
      "catalog_uuid": "db575d07-9b80-4d82-a782-475098085349",
      "name": certname,
      "catalog_format": 1,
      "environment": environment,
      "code_id": nil,
      "version": 1590015651,
      "resources": [
        {
          "type": "Stage",
          "title": "main",
          "tags": [
            "stage",
            "class"
          ],
          "exported": false,
          "parameters": {
            "name": "main"
          }
        },
        {
          "type": "Class",
          "title": "Settings",
          "tags": [
            "class",
            "settings"
          ],
          "exported": false
        },
        {
          "type": "Class",
          "title": "main",
          "tags": [
            "class"
          ],
          "exported": false,
          "parameters": {
            "name": "main"
          }
        },
        {
          "type": "File",
          "title": file_path,
          "tags": [
            "file",
            "class"
          ],
          "file": "/opt/puppetlabs/server/data/orchestration-services/code/environments/plan_testing_env/modules/run_plan_apply/plans/file_content.pp",
          "line": 7,
          "exported": false,
          "parameters": {
            "ensure": "file",
            "source": "puppet:///modules/basic/data"
          }
        }
      ],
      "edges": [
        {
          "source": "Stage[main]",
          "target": "Class[Settings]"
        },
        {
          "source": "Stage[main]",
          "target": "Class[main]"
        },
        {
          "source": "Class[main]",
          "target": "File[#{file_path}]"
        }
      ],
      "classes": [
        "settings"
      ]
    }
  }
end

test_name 'run script tests' do

  tag 'audit:high',      # module validation: no other venue exists to test
      'audit:acceptance'

  extend Puppet::Acceptance::EnvironmentUtils
  step 'add module with required plugins to apply environment' do
    test_module = 'basic'
    puppetserver_env_dir = '/etc/puppetlabs/code/environments/apply'
    fixtures = File.absolute_path('files')
    moduledir = File.join(puppetserver_env_dir, 'modules')
    on(master, "mkdir -p #{moduledir}")
    scp_to(master, File.join(fixtures, "modules/#{test_module}"), moduledir)
    create_remote_file(master, File.join(puppetserver_env_dir, 'environment.conf'), '')
    on(master, "chmod 0644 #{puppetserver_env_dir}/environment.conf")
    on(master, "chmod 0644 #{puppetserver_env_dir}/modules/#{test_module}/files/data")

    teardown do
      on master, "rm -rf #{puppetserver_env_dir}"
    end
  end

  step 'Ensure each agent host has pxp-agent running and associated' do
    agents.each do |agent|
      on agent, puppet('resource service pxp-agent ensure=stopped')
      create_remote_file(agent, pxp_agent_config_file(agent), pxp_config_hocon_using_puppet_certs(master, agent))
      on agent, puppet('resource service pxp-agent ensure=running')

      assert(is_associated?(master, "pcp://#{agent}/agent"),
             "Agent #{agent} with PCP identity pcp://#{agent}/agent should be associated with pcp-broker")
    end
  end

  step 'Execute an apply action with no plugin dependencies' do
    suts.each do |agent|
      catalog_request = apply_catalog_entry(cross_platform_catalog(agent.hostname, 'apply'))
      run_successful_apply(master, agent, catalog_request) do |std_out|
        apply_result = JSON.parse(std_out)
        assert(apply_result['resource_statuses'].include?('Notify[hello world]'), "Agent #{agent} failed to apply catalog")
      end

      teardown {
        clean_files(agent)
      }
    end
  end

  step 'Execute an apply prep' do
    suts.each do |agent|
      apply_prep = { environment: 'apply' }
      run_successful_apply_prep(master, agent, apply_prep) do |std_out|
        prep_result = JSON.parse(std_out)
        assert(prep_result.include?('os'), "Agent #{agent} failed to gather facts")
        assert(prep_result['another'] == "I'm", "Agent #{agent} failed to gather plugin facts")
      end

      teardown {
        clean_files(agent)
      }
    end
  end

  step 'Execute an apply action with plugin dependencies' do
    suts.each do |agent|
      catalog = plugin_dependend_catalog(agent.hostname, 'apply')
      catalog_request = apply_catalog_entry(catalog)
      run_successful_apply(master, agent, catalog_request) do |std_out|
        apply_result = JSON.parse(std_out)
        assert(apply_result['resource_statuses'].include?("Warn[Hello\\!]"), "Agent #{agent} failed to apply catalog")
        assert(apply_result['resource_statuses']["Warn[Hello\\!]"]['failed'] == false, "Agent #{agent} failed to apply catalog")
        assert(apply_result['resource_statuses']["Warn[Hello\\!]"]['failed'] == false, "Agent #{agent} failed to apply catalog")
        event = apply_result['resource_statuses']["Warn[Hello\\!]"]['events'].first
        assert(event['status'] == 'success', "Agent #{agent} failed to apply catalog")
      end

      teardown {
        clean_files(agent)
      }
    end
  end

  step 'Execute an apply action with plugin dependencies' do
    suts.each do |agent|
      windows_agent = windows?(agent)
      target_file = windows_agent ? 'C:/tmp/data' : '/tmp/data'
      if windows_agent
        assert(0 == on(agent, puppet("resource file C:/tmp ensure=directory force=true")).exit_code, "Could not creat tmp file on windows")
      end
      catalog = file_dependent_catalog(agent.hostname, target_file, 'apply')
      catalog_request = apply_catalog_entry(catalog)
      run_successful_apply(master, agent, catalog_request) do |std_out|
        apply_result = JSON.parse(std_out)
        assert(!(apply_result['status'] =~ /failed/), "Failed to apply")
      end
      on(agent, "cat #{target_file}") do result
        assert(result.stdout =~ /file content/)
      end

      teardown {
        clean_files(agent, target_file)
      }
    end
  end
end
