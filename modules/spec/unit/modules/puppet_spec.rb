#!/usr/bin/env rspec

load File.join(File.dirname(__FILE__), "../", "../", "../", "pxp-module-puppet")

describe "pxp-module-puppet" do

  let(:default_input) {
    {"env" => [], "flags" => []}
  }

  let(:temp_dir) {
    "/foo/bar"
  }

  let (:run_report_tempfile) {
    run_report_tempfile = double(Tempfile)
    allow(run_report_tempfile).to receive(:path).and_return(temp_dir + "/baz.yaml")
    run_report_tempfile
  }

  let(:default_configuration) {
    {"puppet_bin" => "puppet", "run_report_tempfile" => run_report_tempfile}
  }

  describe "run_result" do
    it "returns the basic structure with exitcode set" do
      expect(run_result(42)).to be == {"kind"             => "unknown",
                                       "time"             => "unknown",
                                       "transaction_uuid" => "unknown",
                                       "environment"      => "unknown",
                                       "status"           => "unknown",
                                       "exitcode"         => 42,
                                       "version"          => 1}
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
        ["puppet", "agent", "--lastrunreport", run_report_tempfile.path]
    end

    it "should correctly append any flags" do
      input = default_input
      input["flags"] = ["--noop", "--verbose"]
      expect(make_command_array(default_configuration, input)).to be ==
        ["puppet", "agent", "--noop", "--verbose", "--lastrunreport", run_report_tempfile.path]
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
    it "doesn't process the run_report if the file doens't exist" do
      allow(File).to receive(:exist?).and_return(false)
      expect(get_result_from_report(0, default_configuration)).to be ==
          {"kind"             => "unknown",
           "time"             => "unknown",
           "transaction_uuid" => "unknown",
           "environment"      => "unknown",
           "status"           => "unknown",
           "error_type"       => "no_last_run_report",
           "error"            => "/foo/bar/baz.yaml doesn't exist",
           "exitcode"         => 0,
           "version"          => 1}
    end

    it "doesn't process the run_report if the file cant be loaded" do
      allow(File).to receive(:exist?).and_return(true)
      allow(YAML).to receive(:load_file).and_raise("error")
      expect(get_result_from_report(0, default_configuration)).to be ==
          {"kind"             => "unknown",
           "time"             => "unknown",
           "transaction_uuid" => "unknown",
           "environment"      => "unknown",
           "status"           => "unknown",
           "error_type"       => "invalid_last_run_report",
           "error"            =>  "/foo/bar/baz.yaml could not be loaded: error",
           "exitcode"         => 0,
           "version"          => 1}
    end

    it "correctly processes the run_report" do
      run_time = Time.now + 10
      run_report = double(:run_report)

      allow(run_report).to receive(:kind).and_return("apply")
      allow(run_report).to receive(:time).and_return(run_time)
      allow(run_report).to receive(:transaction_uuid).and_return("ac59acbe-6a0f-49c9-8ece-f781a689fda9")
      allow(run_report).to receive(:environment).and_return("production")
      allow(run_report).to receive(:status).and_return("changed")

      allow(File).to receive(:exist?).and_return(true)
      allow(YAML).to receive(:load_file).and_return(run_report)

      expect(get_result_from_report(0, default_configuration)).to be ==
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
      expect_any_instance_of(Object).to receive(:get_result_from_report).with(0, default_configuration)
      start_run(default_configuration, default_input)
    end

    it "populates output when it terminated with a non 0 code" do
      allow(Puppet::Util::Execution).to receive(:execute).and_return(runoutcome)
      allow(runoutcome).to receive(:exitstatus).and_return(1)
      expect_any_instance_of(Object).to receive(:get_result_from_report).with(1, default_configuration)
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
            },
            :temp_dir => {
              :type => "string"
            }
          }
        }
      }
    end
  end

  describe "get_configuration" do
    context "when temp_dir parameter is specified in the configuration" do
      it "leaves the parameter unchanged" do
        expect(Dir).to receive(:tmpdir).never
        allow(Tempfile).to receive(:new).with(anything, "/configured/temp/dir").and_return(run_report_tempfile)
        allow(run_report_tempfile).to receive(:close)
        default_configuration["temp_dir"] = "/configured/temp/dir"
        expect(get_configuration({"configuration" => default_configuration, "input" => default_input})["temp_dir"]).to be ==
          "/configured/temp/dir"
      end
    end

    context "when temp_dir parameter is NOT specified in the configuration" do
      it "set the parameter to the platform temp directory" do
        expect(Dir).to receive(:tmpdir).and_return(temp_dir)
        allow(Tempfile).to receive(:new).with(anything, temp_dir).and_return(run_report_tempfile)
        allow(run_report_tempfile).to receive(:close)
        expect(get_configuration({"configuration" => default_configuration, "input" => default_input})["temp_dir"]).to be ==
          temp_dir
      end
    end

    it "sets the run_report_tempfile parameter to a tempfile created in the temp_dir directory" do
      allow(Dir).to receive(:tmpdir).and_return(temp_dir)
      expect(Tempfile).to receive(:new).with(anything, temp_dir).and_return(run_report_tempfile)
      allow(run_report_tempfile).to receive(:close)
      expect(get_configuration({"configuration" => default_configuration, "input" => default_input})["run_report_tempfile"]).to be ==
        run_report_tempfile
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
      expect(self).to receive(:get_configuration).and_return(default_configuration)
      expect(action_run("{\"input\": null}")["error"]).to be ==
          "The json received on STDIN did not contain a valid 'input' key: {\"input\"=>nil}"
    end

    it "fails when the flags of the passed json data are not all whitelisted" do
      passed_args = {"configuration" => default_configuration,
                     "input" => {"env" => [],
                                 "flags" => ["--prerun_command", "echo safe"]}}
      expect(action_run(passed_args.to_json)["error"]).to be ==
          "The json received on STDIN included a non-permitted flag: --prerun_command"
    end

    it "populates flags with the correct defaults" do
      expected_input = {"env" => [],
                        "flags" => ["--onetime", "--no-daemonize", "--verbose"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect(self).to receive(:get_configuration).and_return(default_configuration)
      expect(self).to receive(:start_run).with(default_configuration, expected_input)
      action_run({"configuration" => default_configuration, "input" => default_input}.to_json)
    end

    it "does not allow changing settings of default flags" do
      passed_args = {"configuration" => default_configuration,
                     "input" => {"env" => [],
                                 "flags" => ["--daemonize"]}}
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
      expect_any_instance_of(Object).to receive(:start_run).with(default_configuration,
                                                                 expected_input)
      action_run({"configuration" => default_configuration, "input" => passed_input}.to_json)
    end

    it "allows passing arguments of flags" do
      expected_input = {"env" => [],
                         "flags" => ["--environment", "/etc/puppetlabs/the_environment",
                                    "--onetime", "--no-daemonize", "--verbose"]}
      passed_input = {"env" => [],
                      "flags" => ["--environment", "/etc/puppetlabs/the_environment"]}
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect(self).to receive(:get_configuration).and_return(default_configuration)
      expect(self).to receive(:start_run).with(default_configuration, expected_input)
      action_run({"configuration" => default_configuration, "input" => passed_input}.to_json)
    end

    it "fails when puppet_bin doesn't exist" do
      allow(File).to receive(:exist?).and_return(false)
      expect(self).to receive(:get_configuration).and_return(default_configuration)
      expect(action_run({"configuration" => default_configuration, "input" => default_input}.to_json)["error"]).to be ==
          "Puppet executable 'puppet' does not exist"
    end

    it "starts the run" do
      allow(File).to receive(:exist?).and_return(true)
      allow_any_instance_of(Object).to receive(:running?).and_return(false)
      allow_any_instance_of(Object).to receive(:disabled?).and_return(false)
      expect(self).to receive(:get_configuration).and_return(default_configuration)
      expect(self).to receive(:start_run)
      action_run({"configuration" => default_configuration, "input" => default_input}.to_json)
    end
  end
end
