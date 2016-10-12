require 'beaker/host_prebuilt_steps'
extend Beaker::HostPrebuiltSteps

# Note: QENG-3641 would allow beaker to take care of this for us
test_name 'Ensure hosts have synchronized clocks'

# BKR-797 - If host is already running ntpd, manually running ntpdate will return 1
# Affects OSX and Ubuntu on vmpooler
applicable_hosts = hosts.select{|host| host['platform'] !~ /osx|ubuntu/}

# BKR-960 - timesync does not work on Cisco XR
applicable_hosts = applicable_hosts.select{|host| host['platform'] !~ /cisco_ios_xr/} 

timesync(applicable_hosts, {:logger => logger})
