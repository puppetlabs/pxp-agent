#!/usr/bin/env ruby

require 'json'
require 'tmpdir'
require 'fileutils'

params = JSON.parse(STDIN.read)

workdir = Dir.mktmpdir
build_host = ''
Dir.chdir(workdir) do
  `git clone git@github.com:puppetlabs/puppet-agent.git 1>&2`
  Dir.chdir('puppet-agent') do
    `git checkout #{params['agent_ref']} 1>&2`
    `bundle install 1>&2`
    `bundle exec build puppet-agent #{params['os_type']} --preserve --only_build=cpp-pcp-client,cpp-hocon 1>&2`
    build_host = File.read('vanagon_hosts.log').match(/Reserving\s[A-Za-z\-\.]*\s/).to_s.gsub('Reserving', '').strip
  end
end

$stdout.puts JSON.generate({'build_host' => build_host})