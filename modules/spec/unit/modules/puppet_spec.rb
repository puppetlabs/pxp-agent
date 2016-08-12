#!/usr/bin/env rspec

load File.join(File.dirname(__FILE__), "../", "../", "../", "pxp-module-puppet")

describe "pxp-module-puppet" do

  let(:default_input) {
    {"flags" => []}
  }

  let(:default_configuration) {
    {"puppet_bin" => "puppet"}
  }

  let(:last_run_report) {
    report = Puppet::Transaction::Report.new('apply', nil, 'production', 'ac59acbe-6a0f-49c9-8ece-f781a689fda9')
    allow(report).to receive(:time).and_return 'a time'
    allow(report).to receive(:status).and_return 'changed'
    allow(report).to receive(:metrics).and_return({
        'resources' => Puppet::Util::Metric.from_data_hash({
        'name' => 'resources',
        'label' => 'Resources',
        'values' => [
          ['total', 'Total', 183],
          ['skipped', 'Skipped', 0],
          ['failed', 'Failed', 0],
          ['failed_to_restart', 'Failed to restart', 0],
          ['restarted', 'Restarted', 0],
          ['changed', 'Changed', 1],
          ['out_of_sync', 'Out of sync', 1],
          ['scheduled', 'Scheduled', 0],
        ],
      })
    })
    report
  }

  describe "last_run_result" do
    it "returns the basic structure with exitcode set" do
      expect(last_run_result(42)).to be == {"kind"             => "unknown",
                                            "time"             => "unknown",
                                            "transaction_uuid" => "unknown",
                                            "environment"      => "unknown",
                                            "status"           => "unknown",
                                            "metrics"          => {},
                                            "exitcode"         => 42,
                                            "version"          => 1}
    end
  end

  describe "running?" do
    it "returns true if the log from a run report states that a run is in progress" do
      run_output = ["Notice: Run of Puppet configuration client already in progress;" +
                    "skipping  (/opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock exists)"]
      expect(running?(run_output)).to be == true
    end

    it "returns false if the log from a run report doesn't state that a run is in progress" do
      run_output = ["Notice: Skipping run of Puppet configuration client;" +
                    "administratively disabled (Reason: 'reason not specified');\n" +
                    "Use 'puppet agent --enable' to re-enable."]
      expect(running?(run_output)).to be == false
    end
  end

  describe "disabled?" do
    it "returns true if the log from a run report states that puppet agent is disabled" do
      run_output = ["Notice: Skipping run of Puppet configuration client;" +
                    "administratively disabled (Reason: 'reason not specified');\n" +
                    "Use 'puppet agent --enable' to re-enable."]
      expect(disabled?(run_output)).to be == true
    end

    it "returns false if the log from a run report doesn't state that puppet is disabled" do
      run_output = ["Notice: Run of Puppet configuration client already in progress;" +
                    "skipping  (/opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock exists)"]
      expect(disabled?(run_output)).to be == false
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
           "metrics"          => {},
           "error_type"       => "agent_failed_to_start",
           "error"            => "test error",
           "exitcode"         => 42,
           "version"          => 1}
    end
  end

  describe "get_result_from_report" do
    it "processes the last_run_report" do
      expect(get_result_from_report(last_run_report, -1)).to be ==
          {"kind"             => "apply",
           "time"             => 'a time',
           "transaction_uuid" => "ac59acbe-6a0f-49c9-8ece-f781a689fda9",
           "environment"      => "production",
           "status"           => "changed",
           "metrics"          => {'total' => 183,
                                  'skipped' => 0,
                                  'failed' => 0,
                                  'failed_to_restart' => 0,
                                  'restarted' => 0,
                                  'changed' => 1,
                                  'out_of_sync' => 1,
                                  'scheduled' => 0,},
           "error_type"       => "agent_exit_non_zero",
           "error"            => "Puppet agent exited with a non 0 exitcode",
           "exitcode"         => -1,
           "version"          => 1}
    end

    it "correctly processes the last_run_report" do
      expect(get_result_from_report(last_run_report, 0)).to be ==
          {"kind"             => "apply",
           "time"             => 'a time',
           "transaction_uuid" => "ac59acbe-6a0f-49c9-8ece-f781a689fda9",
           "environment"      => "production",
           "status"           => "changed",
           "metrics"          => {'total' => 183,
                                  'skipped' => 0,
                                  'failed' => 0,
                                  'failed_to_restart' => 0,
                                  'restarted' => 0,
                                  'changed' => 1,
                                  'out_of_sync' => 1,
                                  'scheduled' => 0,},
           "exitcode"         => 0,
           "version"          => 1}
    end
  end

  describe "start_run" do
    before do
      allow(Puppet).to receive(:initialize_settings)
      allow(Puppet).to receive(:[]).with(anything)
    end

    let(:lockfile) {
      "/opt/puppetlabs/puppet/cache/state/agent_catalog_run.lock"
    }

    it "populates output when it terminated normally" do
      expect_any_instance_of(Puppet::Agent).to receive(:run).and_return(0)
      expect_any_instance_of(Object).to receive(:get_result_from_report).with(anything, 0)
      start_run(default_input)
    end

    it "populates output when it terminated with a non 0 code" do
      expect_any_instance_of(Puppet::Agent).to receive(:run).and_return(1)
      expect_any_instance_of(Object).to receive(:get_result_from_report).with(anything, 1)
      start_run(default_input)
    end

    it "populates the output if puppet is disabled" do
      expect_any_instance_of(Puppet::Agent).to receive(:run).and_return(1)
      expect_any_instance_of(Object).to receive(:disabled?).and_return(true)
      expect_any_instance_of(Object).to receive(:make_error_result).with(1, Errors::Disabled, anything)
      start_run(default_input)
    end

    it "waits for puppet to finish if already running" do
      expect_any_instance_of(Puppet::Agent).to receive(:run).twice.and_return(0)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:running?).and_return(true, false)

      expect(Puppet).to receive(:[]).with(:agent_catalog_run_lockfile).and_return(lockfile)
      expect(File).to receive(:exist?).with('').and_return(false)
      expect(File).to receive(:exist?).with(lockfile).and_return(true, false)

      expect_any_instance_of(Object).to receive(:get_result_from_report).with(anything, 0)
      start_run(default_input)
    end

    it "retries puppet after 30 seconds if lockfile still present" do
      expect_any_instance_of(Puppet::Agent).to receive(:run).twice.and_return(0)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:running?).and_return(true, false)

      expect(Puppet).to receive(:[]).with(:agent_catalog_run_lockfile).and_return(lockfile)
      expect(File).to receive(:exist?).with('').and_return(false)
      expect(File).to receive(:exist?).with(lockfile).exactly(301).times.and_return(true)
      expect_any_instance_of(Object).to receive(:sleep).with(0.1).exactly(300).times

      expect_any_instance_of(Object).to receive(:get_result_from_report).with(anything, 0)
      start_run(default_input)
    end
  end

  describe "get_flag_name" do
    it "returns the flag name" do
      expect(get_flag_name("--spam")).to be == "spam"
    end

    it "returns the flag name in case it's negative" do
      expect(get_flag_name("--no-spam")).to be == "spam"
    end

    it "returns an empty string in case the flag has only a suffix" do
      expect(get_flag_name("--")).to be == ""
      expect(get_flag_name("--no-")).to be == ""
    end

    it "raises an error in case of invalid suffix" do
      expect do
        get_flag_name("-spam")
      end.to raise_error(RuntimeError, /Assertion error: we're here by mistake/)
    end

    it "raises an error in case the flag has no suffix" do
      expect do
        get_flag_name("eggs")
      end.to raise_error(RuntimeError, /Assertion error: we're here by mistake/)
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
                  :items => {
                    :type => "string"
                  }
                }
              },
              :required => [:flags]
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
                :metrics => {
                  :type => "object"
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
    it "fails when invalid json is passed" do
      expect(action_run("{")["error"]).to be ==
          "Invalid json received on STDIN: {"
    end

    it "fails when the passed json data isn't a hash" do
      expect(action_run("[]")["error"]).to be ==
          "The json received on STDIN was not a hash: []"
    end

    it "fails when the passed json data doesn't contain the 'input' key" do
      expect(action_run("{\"input\": null}")["error"]).to be ==
          "The json received on STDIN did not contain a valid 'input' key: {\"input\"=>nil}"
    end

    it "fails when the flags of the passed json data are not all whitelisted" do
      passed_args = {"configuration" => default_configuration,
                     "input" => {"flags" => ["--prerun_command", "echo safe"]}}
      expect(action_run(passed_args.to_json)["error"]).to be ==
          "The json received on STDIN included a non-permitted flag: --prerun_command"
    end

    it "fails when a non-whitelisted flag of the passed json data has whitespace padding" do
      passed_args = {"configuration" => default_configuration,
                     "input" => {"flags" => ["  --prerun_command", "echo safe"]}}
      expect(action_run(passed_args.to_json)["error"]).to be ==
          "The json received on STDIN included a non-permitted flag: --prerun_command"
    end

    it "fails when a non-whitelisted flag of the passed json data has unicode padding" do
      passed_args = {"configuration" => default_configuration,
                     "input" => {"flags" => [" --prerun_command", "echo safe"]}}
      expect(action_run(passed_args.to_json)["error"]).to be ==
          "The json received on STDIN contained characters not present in valid flags:  --prerun_command"
    end

    it "fails when a non-whitelisted flag of the passed json data has escaped padding" do
      passed_args = {"configuration" => default_configuration,
                     "input" => {"flags" => ["\\t--prerun_command", "echo safe"]}}
      expect(action_run(passed_args.to_json)["error"]).to be ==
          "The json received on STDIN contained characters not present in valid flags: \\t--prerun_command"
    end

    it "populates flags with the correct defaults" do
      expected_input = {"flags" => ["--onetime", "--no-daemonize", "--verbose"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run).with(expected_input)
      action_run({"configuration" => default_configuration, "input" => default_input}.to_json)
    end

    it "does not allow changing settings of default flags" do
      passed_args = {"configuration" => default_configuration,
                     "input" => {"flags" => ["--daemonize"]}}
      expect(action_run(passed_args.to_json)["error"]).to be ==
          "The json received on STDIN overrides a default setting with: --daemonize"
    end

    it "does not add flag defaults if they have been passed" do
      expected_input = {"env" => [],
                        "flags" => ["--show_diff", "--onetime", "--no-daemonize", "--verbose"]}
      passed_input = {"env" => [],
                      "flags" => ["--show_diff", "--onetime", "--no-daemonize"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run).with(expected_input)
      action_run({"configuration" => default_configuration, "input" => passed_input}.to_json)
    end

    it "allows passing arguments of flags" do
      expected_input = {"flags" => ["--environment", "the_environment",
                                    "--onetime", "--no-daemonize", "--verbose"]}
      passed_input = {"flags" => ["--environment", "the_environment"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run).with(expected_input)
      action_run({"configuration" => default_configuration, "input" => passed_input}.to_json)
    end

    it "allows passing the obsolete `env` input array" do
      expected_input = {"env" => ["MY_PATH", "/some/path"],
                         "flags" => ["--environment", "the_environment",
                                    "--onetime", "--no-daemonize", "--verbose"]}
      passed_input = {"env" => ["MY_PATH", "/some/path"],
                      "flags" => ["--environment", "the_environment"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run).with(expected_input)
      action_run({"configuration" => default_configuration, "input" => passed_input}.to_json)
    end

    it "starts the run" do
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect_any_instance_of(Object).to receive(:start_run)
      action_run({"configuration" => default_configuration, "input" => default_input}.to_json)
    end
  end
end
