require 'pxp-agent/test_helper.rb'
require 'json'

# Runs a task on targets, and passes the output to the block for validation
# Block should expect a map parsed from the JSON output
def run_task(broker, targets, task, input = {}, max_retries = 30, query_interval = 1, &block)
  target_identities = targets.map {|agent| "pcp://#{agent}/agent"}

  provisional_responses =
    begin
      rpc_non_blocking_request(broker, target_identities,
                               'task', 'run',
                               {:task => task, :input => input})
    rescue => exception
      fail("Exception occurred when trying to run task '#{task}' on all agents: #{exception.message}")
    end

  transaction_ids = target_identities.map do |identity|
    assert_equal("http://puppetlabs.com/rpc_provisional_response",
                 provisional_responses[identity][:envelope][:message_type],
                 "Did not receive expected rpc_provisional_response in reply to non-blocking request")
    provisional_responses[identity][:data]["transaction_id"]
  end

  target_identities.zip(transaction_ids).map do |identity, transaction_id|
    check_non_blocking_response(broker, identity, transaction_id, max_retries, query_interval, &block)
  end
end

def create_task_on(agent, mod, task, body)
  if agent['platform'] =~ /win/
    tasks_path = 'C:/ProgramData/PuppetLabs/pxp-agent/tasks'
  else
    tasks_path = '/opt/puppetlabs/pxp-agent/tasks'
  end

  task_path = "#{tasks_path}/#{mod}/tasks"
  on agent, "mkdir -p #{task_path}"

  create_remote_file(agent, "#{task_path}/#{task}", body)
  on agent, "chmod +x #{task_path}/#{task}"

  teardown do
    on agent, "rm -rf #{task_path}"
  end
end
