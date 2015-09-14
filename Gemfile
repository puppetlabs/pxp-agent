source 'https://rubygems.org'

gem 'puppet', '~> 4.2'

group :test do
  gem 'rspec', '~> 3.1'
end

if File.exists? "#{__FILE__}.local"
  eval(File.read("#{__FILE__}.local"), binding)
end
