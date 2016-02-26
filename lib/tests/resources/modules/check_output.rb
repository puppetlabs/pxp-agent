#!/usr/bin/env ruby

def check_output_files(args)
  output_files = args["output_files"]
  if !output_files
    return
  end

  $stdout.reopen(File.open(output_files["stdout"], 'w'))
  $stderr.reopen(File.open(output_files["stderr"], 'w'))

  at_exit do
    status = if $!.nil?
      0
    elsif $!.is_a?(SystemExit)
      $!.status
    else
      1
    end
    $stdout.fsync
    $stderr.fsync
    File.open(output_files["exitcode"], 'w') do |f|
      f.puts(status)
    end
  end
end
