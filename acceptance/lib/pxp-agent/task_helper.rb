require 'pxp-agent/test_helper.rb'
require 'json'
require 'digest'

def run_task(broker, target_identities, task, filename, sha256, input, input_method, path, rpc_request, expected_response_type, &check_datas)
  responses =
    begin
      data = {:task => task,
              :input => input,
              :files => [{:uri => {:path => path, :params => {}},
                          :filename => filename,
                          :sha256 => sha256}]}
      data[:input_method] = input_method if input_method
      rpc_request.call(broker, target_identities, 'task', 'run', data)
    rescue => exception
      fail("Exception occurred when trying to run task '#{task}' on all agents: #{exception.message}")
    end

  datas = target_identities.map do |identity|
    assert_equal(expected_response_type,
                 responses[identity][:envelope][:message_type],
                 "Did not receive expected rpc_response in reply to rpc request")

    responses[identity][:data]
  end

  check_datas.call(datas)
end

# Runs a task on targets, and passes the output to the block for validation
# Block should expect a map parsed from the JSON output
def run_successful_task(broker, targets, task, filename, sha256, input = {}, input_method = nil, path = "foo", max_retries = 30, query_interval = 1, &block)
  target_identities = targets.map {|agent| "pcp://#{agent}/agent"}

  run_task(broker, target_identities, task, filename, sha256, input, input_method, path, method(:rpc_non_blocking_request), "http://puppetlabs.com/rpc_provisional_response") do |datas|
    transaction_ids = datas.map { |data| data["transaction_id"] }
    target_identities.zip(transaction_ids).map do |identity, transaction_id|
      check_non_blocking_response(broker, identity, transaction_id, max_retries, query_interval) do |stdout|
        block.call(stdout)
      end
    end
  end
end

# Runs a task that's expected to result in a PXP error. Block should
# expect the description of the PXP error to verify.
def run_pxp_errored_task(broker, targets, task, filename, sha256, input = {}, input_method = nil, path = "foo", &block)
  target_identities = targets.map {|agent| "pcp://#{agent}/agent"}

  run_task(broker, target_identities, task, filename, sha256, input, input_method, path, method(:rpc_blocking_request), "http://puppetlabs.com/rpc_error_message") do |datas|
    descriptions = datas.map { |data| data["description"] }
    descriptions.each { |description| block.call(description) }
  end
end

def get_tasks_cache(agent)
  agent['platform'] =~ /win/ ?
    'C:/ProgramData/PuppetLabs/pxp-agent/tasks-cache'
  : '/opt/puppetlabs/pxp-agent/tasks-cache'
end

def create_task_on(agent, mod, task, body)
  tasks_cache = get_tasks_cache(agent)

  sha256 = Digest::SHA256.hexdigest(body+"\n")
  task_path = "#{tasks_cache}/#{sha256}"
  on agent, "mkdir -p #{task_path}"

  create_remote_file(agent, "#{task_path}/#{task}", body)
  on agent, "chmod +x #{task_path}/#{task}"

  teardown do
    on agent, "rm -rf #{task_path}"
  end

  sha256
end
