require 'hocon/config_value_factory'
# This file contains common strings and functions for working with pxp-agent's config file

PXP_CONFIG_DIR_CYGPATH = '/cygdrive/c/ProgramData/PuppetLabs/pxp-agent/etc/'
PXP_CONFIG_DIR_POSIX   = '/etc/puppetlabs/pxp-agent/'

PCP_BROKER_PORTS       = [8142, 8143]
PCP_BROKER_REPL_PORTS  = [7888, 7889]

PCP_VERSION = ENV['PCP_VERSION'] || '2'

HOCON_RENDER_OPTIONS = Hocon::ConfigRenderOptions.new(
  false,  # origin_comments?
  true,   # comments?
  true,   # formatted?
  false,  # json?
  :colon, # key_value_separator (:colon or :equals)
)

def to_hocon(config)
  Hocon::ConfigValueFactory.from_any_ref(config).render(HOCON_RENDER_OPTIONS)
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
      return "/cygdrive/c/Program\\ Files\\ \\(x86\\)/Puppet\\ Labs/Puppet/puppet/bin"
    end
    return "/cygdrive/c/Program\\ Files/Puppet\\ Labs/Puppet/puppet/bin"
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
# @return a string contianing the pxp-agent config in the HOCON syntax
# @raise ArgumentError if one of the mandatory config keys is not passed into the method
def pxp_config_hocon(broker, agent, ssl_config = {})
  mandatory_config_keys = [:ssl_key, :ssl_ca_cert, :ssl_cert, :ssl_crl]
  missing_config = mandatory_config_keys - ssl_config.keys
  if (missing_config != [])
    raise ArgumentError.new("Mandatory SSL config values are missing from ssl_config passsed into method: #{missing_config.to_s}")
  end
  if not ssl_config[:broker_ws_uri]
    ssl_config[:broker_ws_uri] = broker_ws_uri(broker)
  end
  to_hocon({
    "broker-ws-uri" => ssl_config[:broker_ws_uri],
    "pcp-version" => PCP_VERSION,
    "loglevel" => logger.is_debug? ? "debug" : "info",
    "ssl-key" => ssl_config[:ssl_key],
    "ssl-crl" => ssl_config[:ssl_crl],
    "ssl-ca-cert" => ssl_config[:ssl_ca_cert],
    "ssl-cert" => ssl_config[:ssl_cert]
  })
end

def pxp_config_hocon_using_puppet_certs(broker, agent, num_brokers: 1, master_proxy: "", broker_proxy: "")
  to_hocon(pxp_config_hash_using_puppet_certs(broker, agent, num_brokers: num_brokers, master_proxy: master_proxy, broker_proxy: broker_proxy))
end

def pxp_config_hash_using_puppet_certs(broker, agent, num_brokers: 1, master_proxy: "", broker_proxy: "")
  broker_uris = []
  for i in 1..num_brokers
    broker_uris << broker_ws_uri(master).sub!(PCP_BROKER_PORTS[0].to_s,PCP_BROKER_PORTS[i-1].to_s)
  end

  on(agent, puppet('config print ssldir')) do |result|
    puppet_ssldir = result.stdout.chomp
    return {
      "broker-ws-uris" => broker_uris,
      "pcp-version" => PCP_VERSION,
      "loglevel" => logger.is_debug? ? "debug" : "info",
      "ssl-key" => "#{puppet_ssldir}/private_keys/#{agent}.pem",
      "ssl-crl" => "#{puppet_ssldir}/crl.pem",
      "ssl-ca-cert" => "#{puppet_ssldir}/certs/ca.pem",
      "ssl-cert" => "#{puppet_ssldir}/certs/#{agent}.pem",
      "master-uris" => ["#{master}"],
      "master-proxy" => master_proxy,
      "broker-ws-proxy" => broker_proxy
    }
  end
end

# @param broker_host the host name or beaker host object for the pcp-broker host
# @return the broker-ws-uri config string
def broker_ws_uri(broker_host, version = nil)
  version = version || PCP_VERSION.to_i
  if version == 2
    path = 'pcp2'
  else
    path = 'pcp'
  end

  if broker_host.is_a?(Unix::Host) && broker_host[:pcp_broker_instance]
    "wss://#{broker_host}:#{PCP_BROKER_PORTS[broker_host[:pcp_broker_instance]]}/#{path}/"
  else
    "wss://#{broker_host}:#{PCP_BROKER_PORTS[0]}/#{path}/"
  end
end

# @param host the beaker host (to determine the correct path for the OS)
# @return path to the pxp-agent.conf file for that host (a cygpath in the case of Windows)
def pxp_agent_config_file(host)
  windows?(host)?
    "#{PXP_CONFIG_DIR_CYGPATH}pxp-agent.conf" :
    "#{PXP_CONFIG_DIR_POSIX}pxp-agent.conf"
end

def configure_pcp_broker(host,instance)
  set_broker_port(host,instance)
  set_broker_nrepl_port(host,instance)
end

def set_broker_port(host,instance)
  broker_config_path = "#{GIT_CLONE_FOLDER}/pcp-broker#{instance}/test-resources/conf.d/webserver.conf"
  port_config_name  = 'ssl-port'
  broker_port = PCP_BROKER_PORTS[instance]
  on(host, "sed --in-place'' --regexp-extended 's/#{port_config_name}:\\s?[0-9]+/#{port_config_name}: #{broker_port}/' #{broker_config_path}")
end

def set_broker_nrepl_port(host,instance)
  nrepl_config_path = "#{GIT_CLONE_FOLDER}/pcp-broker#{instance}/test-resources/conf.d/nrepl.conf"
  port_config_name  = 'port'
  nrepl_port  = PCP_BROKER_REPL_PORTS[instance]
  on(host, "sed --in-place'' --regexp-extended 's/#{port_config_name}\\s?=\\s?[0-9]+/#{port_config_name} = #{nrepl_port}/' #{nrepl_config_path}")
end
