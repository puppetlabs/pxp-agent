require 'beaker/host_prebuilt_steps'
extend Beaker::HostPrebuiltSteps

# Note: QENG-3641 would allow beaker to take care of this for us
test_name 'Ensure hosts have synchronized clocks'

# QENG-3786 - OSX is already running ntpd and manually running ntpdate will return 1
applicable_hosts = hosts.select{|host| host['platform'] !~ /osx/}
timesync(applicable_hosts, {:logger => logger})
