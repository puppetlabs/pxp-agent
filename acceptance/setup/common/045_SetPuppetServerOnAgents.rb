test_name 'Configure puppet server on each agent'

on agents, puppet("config set server #{master} --section agent")
