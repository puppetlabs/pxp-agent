# Start puppetserver - this is a separate pre-suite block because many pre-suites don't actually want/need the server running
on(master, puppet('resource', 'service', 'puppetserver', 'ensure=running', 'enable=true'))
