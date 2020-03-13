Puppet::Functions.create_function(:'pxp_root') do
  def pxp_root
    File.absolute_path(File.join(__dir__, "..", "..", "..", "..", "..", ".."))
  end
end