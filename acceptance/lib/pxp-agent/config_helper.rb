require 'json'
# This file contains common strings and functions for pxp-agent acceptance tests to use

PXP_CONFIG_DIR_CYGPATH = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/etc/'
PXP_CONFIG_DIR_POSIX = '/etc/puppetlabs/pxp-agent/'

PXP_LOG_FILE_CYGPATH = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/var/log/pxp-agent.log'
PXP_LOG_FILE_POSIX = '/var/log/puppetlabs/pxp-agent/pxp-agent.log'

PCP_BROKER_PORT = 8142

# SSL directories and files for our standard set of test certs
SSL_BASE_DIR_WINDOWS = "C:\\\\cygwin64\\\\home\\\\Administrator\\\\test-resources\\\\ssl\\\\"
SSL_BASE_DIR_POSIX = "/root/test-resources/ssl/"
ALT_SSL_BASE_DIR_WINDOWS = "C:\\\\cygwin64\\\\home\\\\Administrator\\\\test-resources\\\\ssl\\\\alternative\\\\"
ALT_SSL_BASE_DIR_POSIX = "/root/test-resources/ssl/alternative/"
SSL_KEY_FILE_DIR_WINDOWS = "private_keys\\\\"
SSL_KEY_FILE_DIR_POSIX = "private_keys/"
SSL_CA_FILE_DIR_WINDOWS = "ca\\\\"
SSL_CA_FILE_DIR_POSIX = "ca/"
SSL_CERT_FILE_DIR_WINDOWS = "certs\\\\"
SSL_CERT_FILE_DIR_POSIX = "certs/"

# Our standard set of test certs use two digits in the filename
# so we need to pad with zero if a single digit number was passed
def left_pad_with_zero(number)
  if (number.to_s.length == 1)
    "0#{number}"
  else
    number
  end
end

def windows?(host)
  host.platform.upcase.start_with?('WINDOWS')
end

# @param host the beaker host object you want the path on
# @return the path to the directory that pxp-agent is located in. A cygpath is returned in the case of Windows.
def pxp_agent_dir(host)
  if (windows?(host))
    # If 32bit Puppet on 64bit Windows then Puppet will be in Program Files (x86)
    if((host.is_x86_64?) && (host[:ruby_arch] == "x86"))
      return "/cygdrive/c/Program\\ Files\\ \\(x86\\)/Puppet\\ Labs/Puppet/pxp-agent/bin"
    end
    return "/cygdrive/c/Program\\ Files/Puppet\\ Labs/Puppet/pxp-agent/bin"
  end
  "/opt/puppetlabs/puppet/bin"
end

# @param broker hostname or beaker host object of the machine running pcp-broker
# @param agent beaker host object of the agent machine that will receive this config
# @param ssl_config hash of strings for the config keys
#                   :broker_ws_uri - the pcp-broker uri (optional, will default using the broker hostname if not set)
#                   :ssl_key - the private key file (mandatory)
#                   :ssl_ca_cert - the ca file for the ssl keys (mandatory)
#                   :ssl_cert - the public cert file (mandatory)
# @return JSON object with pxp-agent config
# @raise ArgumentError if one of the mandatory config keys is not passed into the method
def pxp_config_json(broker, agent, ssl_config = {})
  mandatory_config_keys = [:ssl_key, :ssl_ca_cert, :ssl_cert]
  missing_config = mandatory_config_keys - ssl_config.keys
  if (missing_config != [])
    raise ArgumentError.new("Mandatory SSL config values are missing from ssl_config passsed into method: #{missing_config.to_s}")
  end
  if not ssl_config[:broker_ws_uri]
    ssl_config[:broker_ws_uri] = broker_ws_uri(broker)
  end
  { "broker-ws-uri" => ssl_config[:broker_ws_uri],
    "ssl-key" => ssl_config[:ssl_key],
    "ssl-ca-cert" => ssl_config[:ssl_ca_cert],
    "ssl-cert" => ssl_config[:ssl_cert]
  }.to_json
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 from our standard test certs to use
# @return pxp-agent config as a JSON object, using our standard test cert files
def pxp_config_json_using_test_certs(broker, agent, client_number)
  pxp_config_hash_using_test_certs(broker, agent, client_number).to_json
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 from our standard test certs to use
# @return pxp-agent config as a hash, using our standard test cert files
def pxp_config_hash_using_test_certs(broker, agent, client_number)
  { "broker-ws-uri" => "#{broker_ws_uri(broker)}",
    "ssl-key" => "#{ssl_key_file(agent, client_number)}",
    "ssl-ca-cert" => "#{ssl_ca_file(agent)}",
    "ssl-cert" => "#{ssl_cert_file(agent, client_number)}"
  }
end

# @param broker_host the host name or beaker host object for the pcp-broker host
# @return the broker-ws-uri config string
def broker_ws_uri(broker_host)
  "wss://#{broker_host}:#{PCP_BROKER_PORT}/pcp/"
end

# @param host the beaker host you want the path to the log file on
# @return The path to the log file on the host. For Windows, a cygpath is returned
def logfile(host)
  windows?(host)?
    PXP_LOG_FILE_CYGPATH :
    PXP_LOG_FILE_POSIX
end

# @param host the beaker host (to determine the correct path for the OS)
# @return path to the pxp-agent.conf file for that host (a cygpath in the case of Windows)
def pxp_agent_config_file(host)
  windows?(host)?
    "#{PXP_CONFIG_DIR_CYGPATH}pxp-agent.conf" :
    "#{PXP_CONFIG_DIR_POSIX}pxp-agent.conf"
end

def ssl_key_file_windows(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{SSL_BASE_DIR_WINDOWS}#{SSL_KEY_FILE_DIR_WINDOWS}client#{client_number}.example.com.pem"
end

def ssl_key_file_posix(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{SSL_BASE_DIR_POSIX}#{SSL_KEY_FILE_DIR_POSIX}client#{client_number}.example.com.pem"
end

# @param host a beaker host (to determine the correct path for the OS)
# @param client_number which client 01-05 you want the key file for
# @return the path to the client ssl key file on this host
def ssl_key_file(host, client_number)
  windows?(host)?
    ssl_key_file_windows(client_number) :
    ssl_key_file_posix(client_number)
end

def alt_ssl_key_file_windows(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{ALT_SSL_BASE_DIR_WINDOWS}#{SSL_KEY_FILE_DIR_WINDOWS}client#{client_number}.alt.example.com.pem"
end

def alt_ssl_key_file_posix(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{ALT_SSL_BASE_DIR_POSIX}#{SSL_KEY_FILE_DIR_POSIX}client#{client_number}.alt.example.com.pem"
end

# @param host the beaker host (to determine the correct path for the OS)
# @param client_number which client 01-05 you want the key file for
# @return the path to the alternative client ssl key file on this host
def alt_ssl_key_file(host, client_number)
  windows?(host)?
    alt_ssl_key_file_windows(client_number) :
    alt_ssl_key_file_posix(client_number)
end

def ssl_ca_file_windows
  "#{SSL_BASE_DIR_WINDOWS}#{SSL_CA_FILE_DIR_WINDOWS}ca_crt.pem"
end

def ssl_ca_file_posix
  "#{SSL_BASE_DIR_POSIX}#{SSL_CA_FILE_DIR_POSIX}ca_crt.pem"
end

# @param host the beaker host (to determine the correct path for the OS)
# @return the path to the ssl ca file on this host
def ssl_ca_file(host)
  windows?(host)?
    ssl_ca_file_windows :
    ssl_ca_file_posix
end

def alt_ssl_ca_file_windows
  "#{ALT_SSL_BASE_DIR_WINDOWS}#{SSL_CA_FILE_DIR_WINDOWS}ca_crt.pem"
end

def alt_ssl_ca_file_posix
  "#{ALT_SSL_BASE_DIR_POSIX}#{SSL_CA_FILE_DIR_POSIX}ca_crt.pem"
end

# @param host the beaker host (to determine the correct path for the OS)
# @return the path to the alternative ssl ca file on this host
def alt_ssl_ca_file(host)
  windows?(host)?
    alt_ssl_ca_file_windows :
    alt_ssl_ca_file_posix
end

def ssl_cert_file_windows(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{SSL_BASE_DIR_WINDOWS}#{SSL_CERT_FILE_DIR_WINDOWS}client#{client_number}.example.com.pem"
end

def ssl_cert_file_posix(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{SSL_BASE_DIR_POSIX}#{SSL_CERT_FILE_DIR_POSIX}client#{client_number}.example.com.pem"
end

# @param host the beaker host (to determine the correct path for the OS)
# @param client_number which client 01-05 you want the key file for
# @return the path to the ssl cert for this client on this host
def ssl_cert_file(host, client_number)
  windows?(host)?
    ssl_cert_file_windows(client_number) :
    ssl_cert_file_posix(client_number)
end

def alt_ssl_cert_file_windows(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{ALT_SSL_BASE_DIR_WINDOWS}#{SSL_CERT_FILE_DIR_WINDOWS}client#{client_number}.alt.example.com.pem"
end

def alt_ssl_cert_file_posix(client_number)
  client_number = left_pad_with_zero(client_number)
  "#{ALT_SSL_BASE_DIR_POSIX}#{SSL_CERT_FILE_DIR_POSIX}client#{client_number}.alt.example.com.pem"
end

# @param host the beaker host (to determine the correct path for the OS)
# @param client_number which client 01-05 you want the key file for
# @return the path to the alternative ssl cert for this client on this host
def alt_ssl_cert_file(host, client_number)
  windows?(host)?
    alt_ssl_cert_file_windows(client_number) :
    alt_ssl_cert_file_posix(client_number)
end
