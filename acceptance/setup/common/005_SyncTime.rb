require 'beaker/host_prebuilt_steps'
extend Beaker::HostPrebuiltSteps

# Note: QENG-3641 would allow beaker to take care of this for us
test_name 'Ensure hosts have synchronized clocks'

timesync(hosts, {:logger => logger})
