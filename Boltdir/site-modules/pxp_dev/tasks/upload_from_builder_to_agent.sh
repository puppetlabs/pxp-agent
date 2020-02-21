#!/usr/bin/env sh

for AGENT in $PT_agents
do
  scp -o StrictHostKeyChecking=no /opt/puppetlabs/puppet/bin/pxp-agent "$AGENT:/opt/puppetlabs/puppet/bin/pxp-agent"
  scp -o StrictHostKeyChecking=no -r /opt/puppetlabs/pxp-agent "$AGENT:/opt/puppetlabs/pxp-agent"
done