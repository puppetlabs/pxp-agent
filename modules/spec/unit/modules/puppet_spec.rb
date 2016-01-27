#!/usr/bin/env rspec

load File.join(File.dirname(__FILE__), "../", "../", "../", "pxp-module-puppet")

describe "pxp-module-puppet" do

  let(:default_input) {
    {"env" => [], "flags" => []}
  }

  let(:default_configuration) {
    {"puppet_bin" => "puppet"}
  }

  describe "last_run_result" do
    it "returns the basic structure with exitcode set" do
      expect(last_run_result(42)).to be == {"kind"             => "unknown",
                                            "time"             => "unknown",
                                            "transaction_uuid" => "unknown",
                                            "environment"      => "unknown",
                                            "status"           => "unknown",
                                            "exitcode"         => 42,
                                            "version"          => 1}
    end
  end

  describe "check_config_print" do
    it "returns the result of configprint" do
      cli_vec = ["puppet", "agent", "--configprint", "value"]
      expect(Puppet::Util::Execution).to receive(:execute).with(cli_vec).and_return("value\n")
      expect(check_config_print("value", default_configuration)).to be == "value"
    end
  end

  describe "running?" do
    it "returns true if the output from a run states that a run is in progress" do
      run_output = "Notice: Run of Puppet configuration client already in progress;" +
                   "skipping  (/opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock exists)"
      expect(running?(run_output)).to be == true
    end

    it "returns false if the ouput from a run doesn't state that a run is in progress" do
      run_output = "Notice: Skipping run of Puppet configuration client;" +
                   "administratively disabled (Reason: 'reason not specified');\n" +
                   "Use 'puppet agent --enable' to re-enable."
      expect(running?(run_output)).to be == false
    end
  end

  describe "disabled?" do
    it "returns true if the output from a run states that puppet agent is disabled" do
      run_output = "Notice: Skipping run of Puppet configuration client;" +
                   "administratively disabled (Reason: 'reason not specified');\n" +
                   "Use 'puppet agent --enable' to re-enable."
      expect(disabled?(run_output)).to be == true
    end

    it "returns false if the output from a run doesn't state that puppet is disabled" do
      run_output = "Notice: Run of Puppet configuration client already in progress;" +
                   "skipping  (/opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock exists)"
      expect(disabled?(run_output)).to be == false
    end
  end

  describe "make_environment_hash" do
    it "should correctly add the env entries" do
      input = default_input
      input["env"] = ["FOO=bar", "BAR=foo"]
      expect(make_environment_hash(input)).to be ==
        {"FOO" => "bar", "BAR" => "foo"}
    end
  end

  describe "make_command_array" do
    it "should correctly append the executable and action" do
      input = default_input
      expect(make_command_array(default_configuration, input)).to be ==
        ["puppet", "agent"]
    end

    it "should correctly append any flags" do
      input = default_input
      input["flags"] = ["--noop", "--verbose"]
      expect(make_command_array(default_configuration, input)).to be ==
        ["puppet", "agent", "--noop", "--verbose"]
    end
  end

  describe "make_error_result" do
    it "should set the exitcode, error_type and error_message" do
      expect(make_error_result(42, Errors::FailedToStart, "test error")).to be ==
          {"kind"             => "unknown",
           "time"             => "unknown",
           "transaction_uuid" => "unknown",
           "environment"      => "unknown",
           "status"           => "unknown",
           "error_type"       => "agent_failed_to_start",
           "error"            => "test error",
           "exitcode"         => 42,
           "version"          => 1}
    end
  end

  describe "get_result_from_report" do
    it "doesn't process the last_run_report if the file doens't exist" do
      allow(File).to receive(:exist?).and_return(false)
      allow_any_instance_of(Object).to receive(:check_config_print).and_return("/opt/puppetlabs/puppet/cache/state/last_run_report.yaml")
      expect(get_result_from_report(0, default_configuration, Time.now)).to be ==
          {"kind"             => "unknown",
           "time"             => "unknown",
           "transaction_uuid" => "unknown",
           "environment"      => "unknown",
           "status"           => "unknown",
           "error_type"       => "no_last_run_report",
           "error"            => "/opt/puppetlabs/puppet/cache/state/last_run_report.yaml doesn't exist",
           "exitcode"         => 0,
           "version"          => 1}
    end

    it "doesn't process the last_run_report if the file cant be loaded" do
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:check_config_print).and_return("/opt/puppetlabs/puppet/cache/state/last_run_report.yaml")
      allow(YAML).to receive(:load_file).and_raise("error")
      expect(get_result_from_report(0, default_configuration, Time.now)).to be ==
          {"kind"             => "unknown",
           "time"             => "unknown",
           "transaction_uuid" => "unknown",
           "environment"      => "unknown",
           "status"           => "unknown",
           "error_type"       => "invalid_last_run_report",
           "error"            => "/opt/puppetlabs/puppet/cache/state/last_run_report.yaml could not be loaded: error",
           "exitcode"         => 0,
           "version"          => 1}
    end

    it "doesn't process the last_run_report if it hasn't been updated after the run was kicked" do
      start_time = Time.now
      run_time = Time.now - 10
      last_run_report = double(:last_run_report)

      allow(last_run_report).to receive(:kind).and_return("apply")
      allow(last_run_report).to receive(:time).and_return(run_time)
      allow(last_run_report).to receive(:transaction_uuid).and_return("ac59acbe-6a0f-49c9-8ece-f781a689fda9")
      allow(last_run_report).to receive(:environment).and_return("production")
      allow(last_run_report).to receive(:status).and_return("changed")

      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:check_config_print).and_return("/opt/puppetlabs/puppet/cache/state/last_run_report.yaml")
      allow(YAML).to receive(:load_file).and_return(last_run_report)

      expect(get_result_from_report(-1, default_configuration, start_time)).to be ==
          {"kind"             => "unknown",
           "time"             => "unknown",
           "transaction_uuid" => "unknown",
           "environment"      => "unknown",
           "status"           => "unknown",
           "error_type"       => "agent_exit_non_zero",
           "error"            => "Puppet agent exited with a non 0 exitcode",
           "exitcode"         => -1,
           "version"          => 1}
    end

    it "processes the last_run_report if it has been updated after the run was kicked" do
      start_time = Time.now
      run_time = Time.now + 10
      last_run_report = double(:last_run_report)

      allow(last_run_report).to receive(:kind).and_return("apply")
      allow(last_run_report).to receive(:time).and_return(run_time)
      allow(last_run_report).to receive(:transaction_uuid).and_return("ac59acbe-6a0f-49c9-8ece-f781a689fda9")
      allow(last_run_report).to receive(:environment).and_return("production")
      allow(last_run_report).to receive(:status).and_return("changed")

      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:check_config_print).and_return("/opt/puppetlabs/puppet/cache/state/last_run_report.yaml")
      allow(YAML).to receive(:load_file).and_return(last_run_report)

      expect(get_result_from_report(-1, default_configuration, start_time)).to be ==
          {"kind"             => "apply",
           "time"             => run_time,
           "transaction_uuid" => "ac59acbe-6a0f-49c9-8ece-f781a689fda9",
           "environment"      => "production",
           "status"           => "changed",
           "error_type"       => "agent_exit_non_zero",
           "error"            => "Puppet agent exited with a non 0 exitcode",
           "exitcode"         => -1,
           "version"          => 1}
    end

    it "correctly processes the last_run_report" do
      start_time = Time.now
      run_time = Time.now + 10
      last_run_report = double(:last_run_report)

      allow(last_run_report).to receive(:kind).and_return("apply")
      allow(last_run_report).to receive(:time).and_return(run_time)
      allow(last_run_report).to receive(:transaction_uuid).and_return("ac59acbe-6a0f-49c9-8ece-f781a689fda9")
      allow(last_run_report).to receive(:environment).and_return("production")
      allow(last_run_report).to receive(:status).and_return("changed")

      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:check_config_print).and_return("/opt/puppetlabs/puppet/cache/state/last_run_report.yaml")
      allow(YAML).to receive(:load_file).and_return(last_run_report)

      expect(get_result_from_report(0, default_configuration, start_time)).to be ==
          {"kind"             => "apply",
           "time"             => run_time,
           "transaction_uuid" => "ac59acbe-6a0f-49c9-8ece-f781a689fda9",
           "environment"      => "production",
           "status"           => "changed",
           "exitcode"         => 0,
           "version"          => 1}
    end
  end

  describe "start_run" do
    let(:runoutcome) {
      double(:runoutcome)
    }

    it "populates output when it terminated normally" do
      allow(Puppet::Util::Execution).to receive(:execute).and_return(runoutcome)
      allow(runoutcome).to receive(:exitstatus).and_return(0)
      expect_any_instance_of(Object).to receive(:get_result_from_report).with(0, default_configuration, anything)
      start_run(default_configuration, default_input)
    end

    it "populates output when it terminated with a non 0 code" do
      allow(Puppet::Util::Execution).to receive(:execute).and_return(runoutcome)
      allow(runoutcome).to receive(:exitstatus).and_return(1)
      expect_any_instance_of(Object).to receive(:get_result_from_report).with(1, default_configuration, anything)
      start_run(default_configuration, default_input)
    end

    it "populates output when it couldn't start" do
      allow(Puppet::Util::Execution).to receive(:execute).and_return(nil)
      expect_any_instance_of(Object).to receive(:make_error_result).with(-1, Errors::FailedToStart, anything)
      start_run(default_configuration, default_input)
    end

    it "populates the output if puppet is disabled" do
      allow(Puppet::Util::Execution).to receive(:execute).and_return(runoutcome)
      allow(runoutcome).to receive(:exitstatus).and_return(1)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(true)
      expect_any_instance_of(Object).to receive(:make_error_result).with(1, Errors::Disabled, anything)
      start_run(default_configuration, default_input)
    end

    it "populates the output when puppet is alreaady running" do
      allow(Puppet::Util::Execution).to receive(:execute).and_return(runoutcome)
      allow(runoutcome).to receive(:exitstatus).and_return(1)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      allow_any_instance_of(Object).to receive(:running?).and_return(true)
      expect_any_instance_of(Object).to receive(:make_error_result).with(1, Errors::AlreadyRunning, anything)
      start_run(default_configuration, default_input)
    end
  end

  describe "action_metadata" do
    it "has the correct metadata" do
      expect(metadata).to be == {
        :description => "PXP Puppet module",
        :actions => [
          { :name        => "run",
            :description => "Start a Puppet run",
            :input       => {
              :type      => "object",
              :properties => {
                :env => {
                  :type => "array",
                },
                :flags => {
                  :type => "array",
                }
              },
              :required => [:env, :flags]
            },
            :results => {
              :type => "object",
              :properties => {
                :kind => {
                  :type => "string"
                },
                :time => {
                  :type => "string"
                },
                :transaction_uuid => {
                  :type => "string"
                },
                :environment => {
                  :type => "string"
                },
                :status => {
                  :type => "string"
                },
                :error_type => {
                  :type => "string"
                },
                :error => {
                  :type => "string"
                },
                :exitcode => {
                  :type => "number"
                },
                :version => {
                  :type => "number"
                }
              },
              :required => [:kind, :time, :transaction_uuid, :environment, :status,
                            :exitcode, :version]
            }
          }
        ],
        :configuration => {
          :type => "object",
          :properties => {
            :puppet_bin => {
              :type => "string"
            }
          }
        }
      }
    end
  end

  describe "action_run" do
    it "fails when puppet_bin doesn't exist" do
      allow(File).to receive(:exist?).and_return(false)
      expect(action_run({"configuration" => default_configuration, "input" => default_input})["error"]).to be ==
          "Puppet executable 'puppet' does not exist"
    end

    it "fails when invalid json is passed" do
      # JSON is parsed when the action is invoked. Any invalid json will call run
      # with nil
      expect(action_run(nil)["error"]).to be ==
          "Invalid json parsed on STDIN. Cannot start run action"
    end

    it "populates flags with the correct defaults" do
      expected_input = {"env" => [],
                        "flags" => ["--onetime", "--no-daemonize", "--verbose"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run).with(default_configuration,
                                                                 expected_input)
      action_run({"configuration" => default_configuration, "input" => default_input})
    end

    it "does not add flag defaults if they have been passed" do
      expected_input = {"env" => [],
                        "flags" => ["--squirrels", "--onetime", "--no-daemonize", "--verbose"]}
      passed_input = {"env" => [],
                      "flags" => ["--squirrels", "--onetime", "--no-daemonize"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run).with(default_configuration,
                                                                 expected_input)
      action_run({"configuration" => default_configuration, "input" => passed_input})
    end

    it "starts the run" do
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run)
      action_run({"configuration" => default_configuration, "input" => default_input})
    end
  end
end
