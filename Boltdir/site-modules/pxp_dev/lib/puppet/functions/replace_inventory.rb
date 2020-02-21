Puppet::Functions.create_function(:'replace_inventory') do
  def replace_inventory(target_hash, path_to_inventory)
    contents = { 'version' => 2,
                 'targets' => [target_hash] }.to_yaml
    if File.exists?(path_to_inventory)
      File.delete(path_to_inventory)
    end
    File.open(path_to_inventory,"w") do |f|
     f.write(contents)
    end
  end
end