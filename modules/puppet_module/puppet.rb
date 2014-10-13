require 'puppet'

class PuppetManager

  def initialize
    Puppet.settings.app_defaults_initialized?
    require 'puppet/util/run_mode'
    Puppet.settings.preferred_run_mode = :agent

    args = []
    #(args << "--config=/etc/puppet/puppet.conf")
    Puppet.settings.initialize_global_settings(args)
    Puppet.settings.initialize_app_defaults(
      Puppet::Settings.app_defaults_for_run_mode(Puppet.run_mode))
    # This check is to keep backwards compatibility
    # with Puppet versions < 3.5.0
    if Puppet.respond_to?(:base_context) && Puppet.respond_to?(:push_context)
      Puppet.push_context(Puppet.base_context(Puppet.settings))
    end
  end

  def status
    summary = load_summary
    last_run = Integer(summary["time"].fetch("last_run", 0))
    status = { :applying => applying?,
               :enabled => enabled?,
               :daemon_present => daemon_present?,
               :last_run => last_run,
               :since_lastrun => since_lastrun(last_run),
               :idling => (daemon_present? && !applying?),
               :disable_message => disable_message}

    if !status[:enabled]
      status["status"] = "Enabled"
    elsif status[:applying]
      status["status"] = "Applying a catalog"
    elsif status[:idling]
      status[:status] = "idling"
    elsif !status[:applying]
      status[:status] = "stopped"
    else
      status[:status] = "unknown"
    end

    status
  end

  def run
    if !enabled?
      return {:status => "Cannot start Puppet run. Puppet is disabled"}
    end

    options = ["--onetime", "--no-daemonize", "--color=false",
               "--show_diff", "--verbose"]
    ENV["RUBYLIB"] = "/Library/Ruby/Site/"
    ENV["LANG"] = "en_US.UTF-8"
    ENV["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    { :status => %x[/usr/bin/puppet agent #{options.join(" ")}] }
  end

  private
  def applying?
    File.exist?(Puppet[:agent_catalog_run_lockfile])
  end

  def enabled?
    !File.exist?(Puppet[:agent_disabled_lockfile])
  end

  def daemon_present?
    if File.exist?(Puppet[:pidfile])
      return has_process_for_pid?(File.read(Puppet[:pidfile]))
    end
    false
  end

  def has_process_for_pi?(pid)
    !!::Process.kill(0, Integer(pid)) rescue false
  end

  def since_lastrun(last_run)
    if last_run == 0
      return "Puppet has never run on this host"
    end

    return Time.now.to_i - last_run
  end

  def load_summary
    summary = {"changes" => {},
               "time" => {},
               "resources" => {},
               "version" => {},
               "events" => {}}
    if File.exist?(Puppet[:lastrunfile])
      summary.merge!(YAML.load_file(Puppet[:lastrunfile]))
    end

    summary["resources"] = {"failed" => 0,
                            "changed" => 0,
                            "total" => 0,
                            "restarted" => 0,
                            "out_of_sync" => 0}.merge!(summary["resources"])
    summary
  end

  def disable_message
    if !enabled?
      lock_data = JSON.parse(File.read(Puppet[:agent_disabled_lockfile]))
      return lock_data["disabled_message"]
    end
    return ""
  end

end
