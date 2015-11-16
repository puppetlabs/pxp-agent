require 'json'
# This file contains common strings and functions for pxp-agent acceptance tests to use

PXP_CONFIG_DIR_CYGPATH = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/etc/'
PXP_CONFIG_DIR_POSIX = '/etc/puppetlabs/pxp-agent/'

PCP_BROKER_PORT = 8142

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

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 to use
# @return pxp-agent config as a JSON object
def pxp_config_json(broker, agent, client_number)
  pxp_config_hash(broker, agent, client_number).to_json
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 to use
# @return pxp-agent config as a hash
def pxp_config_hash(broker, agent, client_number)
  { "broker-ws-uri" => "#{broker_ws_uri(broker)}",
    "ssl-key" => "#{ssl_key_file(agent, client_number)}",
    "ssl-ca-cert" => "#{ssl_ca_file(agent)}",
    "ssl-cert" => "#{ssl_cert_file(agent, client_number)}"
  }
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which alternative client cert 01-05 to use
# @return pxp-agent config, using the alternative set of ssl certs, as a JSON object
def pxp_alternative_ssl_config_json(broker, agent, client_number)
  pxp_alternative_ssl_config_hash(broker, agent, client_number).to_json
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which alternative client cert 01-05 to use
# @return pxp-agent config, using the alternative set of ssl certs, as a hash
def pxp_alternative_ssl_config_hash(broker, agent, client_number)
  { "broker-ws-uri" => "#{broker_ws_uri(broker)}",
    "ssl-key" => "#{alt_ssl_key_file(agent, client_number)}",
    "ssl-ca-cert" => "#{alt_ssl_ca_file(agent)}",
    "ssl-cert" => "#{alt_ssl_cert_file(agent, client_number)}"
  }
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 to use
# @return pxp-agent config, with intentionally mismatching ssl cert and key, as a JSON object
def pxp_invalid_config_mismatching_keys_json(broker, agent, client_number)
  pxp_invalid_config_mismatching_keys_hash(broker, agent, client_number).to_json
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 to use
# @return pxp-agent config, with intentionally mismatching ssl cert and key, as a hash
def pxp_invalid_config_mismatching_keys_hash(broker, agent, client_number)
  { "broker-ws-uri" => "#{broker_ws_uri(broker)}",
    "ssl-key" => "#{ssl_key_file(agent, client_number)}",
    "ssl-ca-cert" => "#{ssl_ca_file(agent)}",
    "ssl-cert" => "#{alt_ssl_cert_file(agent, client_number)}"
  }
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 to use
# @return pxp-agent config, with ssl ca cert intentionally not matching its ssl cert and key, as a JSON object
def pxp_invalid_config_wrong_ca_json(broker, agent, client_number)
  pxp_invalid_config_wrong_ca_hash(broker, agent, client_number).to_json
end

# @param broker the host that runs pcp-broker
# @param agent the agent machine that this config is for
# @param client_number which client cert 01-05 to use
# @return pxp-agent config, with ssl ca cert intentionally not matching its ssl cert and key, as a hash
def pxp_invalid_config_wrong_ca_hash(broker, agent, client_number)
  { "broker-ws-uri" => "#{broker_ws_uri(broker)}",
    "ssl-key" => "#{alt_ssl_key_file(agent, client_number)}",
    "ssl-ca-cert" => "#{ssl_ca_file(agent)}",
    "ssl-cert" => "#{alt_ssl_cert_file(agent, client_number)}"
  }
end

# @param broker_host the host name or beaker host object for the pcp-broker host
# @return the broker-ws-uri config string
def broker_ws_uri(broker_host)
  "wss://#{master}:#{PCP_BROKER_PORT}/pcp/"
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
