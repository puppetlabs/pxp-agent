require 'pxp-agent/test_helper.rb'
require 'digest'

def file_entry(filename, sha256, path = 'foo')
  {:uri => {:path => path, :params => {}}, :filename => filename, :sha256 => sha256}
end

def run_task(broker, target_identities, task, input, files, metadata, rpc_request, expected_response_type, &check_datas)
  responses =
    begin
      params = { task: task, input: input, files: files }
      params = params.merge(metadata: metadata) if metadata
      rpc_request.call(broker, target_identities, 'task', 'run', params)
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
def run_successful_task(broker, targets, task, files, input: {}, metadata: nil, max_retries: 30, query_interval: 1, &block)
  target_identities = targets.map {|agent| "pcp://#{agent}/agent"}

  run_task(broker, target_identities, task, input, files, metadata,
           method(:rpc_non_blocking_request), "http://puppetlabs.com/rpc_provisional_response") do |datas|
    transaction_ids = datas.map { |data| data["transaction_id"] }
    target_identities.zip(transaction_ids).map do |identity, transaction_id|
      check_non_blocking_response(broker, identity, transaction_id, max_retries, query_interval) do |result|
        assert_equal('success', result['status'], 'Task was expected to succeed')
        assert_equal(nil, result['stderr'], 'Successful task expected to have no output on stderr')
        assert_equal(0, result['exitcode'], 'Successful task expected to have exit code 0')
        block.call(result['stdout'])
      end
    end
  end
end

def run_failed_task(broker, targets, task, files, input: {}, metadata: nil, max_retries: 30, query_interval: 1, &block)
  target_identities = targets.map {|agent| "pcp://#{agent}/agent"}

  run_task(broker, target_identities, task, input, files, metadata,
           method(:rpc_non_blocking_request), "http://puppetlabs.com/rpc_provisional_response") do |datas|
    transaction_ids = datas.map { |data| data["transaction_id"] }
    target_identities.zip(transaction_ids).map do |identity, transaction_id|
      check_non_blocking_response(broker, identity, transaction_id, max_retries, query_interval) do |result|
        assert_equal('failure', result['status'], 'Task was expected to fail')
        assert_equal(nil, result['stdout'], 'Successful task expected to have no output on stdout')
        assert_equal(1, result['exitcode'], 'Successful task expected to have exit code 1')
        block.call(result['stderr'])
      end
    end
  end
end

# Runs a task that's expected to result in a PXP error. Block should
# expect the description of the PXP error to verify.
def run_pxp_errored_task(broker, targets, task, files, input: {}, metadata: nil, &block)
  target_identities = targets.map {|agent| "pcp://#{agent}/agent"}

  run_task(broker, target_identities, task, input, files, metadata,
           method(:rpc_blocking_request), "http://puppetlabs.com/rpc_error_message") do |datas|
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

  body += "\n" if body[-1] != "\n"
  sha256 = Digest::SHA256.hexdigest(body)
  task_path = "#{tasks_cache}/#{sha256}"
  on agent, "mkdir -p #{task_path}"

  create_remote_file(agent, "#{task_path}/#{task}", body)
  on agent, "chmod +x #{task_path}/#{task}"

  teardown do
    on agent, "rm -rf #{task_path}"
  end

  sha256
end
