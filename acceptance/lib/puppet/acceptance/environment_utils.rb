module Puppet
  module Acceptance
    module EnvironmentUtils

     # generate a random string of 6 letters and numbers.  NOT secure
      def random_string
        [*('a'..'z'),*('0'..'9')].shuffle[0,8].join
      end
      private :random_string

      # if the first test to call this has changed the environmentpath, this will cause trouble
      #   maybe not the best idea to memoize this?
      def environmentpath
        @@memoized_environmentpath ||= master.puppet['environmentpath']
      end

      # create a tmpdir to hold a temporary environment bound by puppet environment naming restrictions
      # symbolically link environment into environmentpath
      def mk_tmp_environment(environment)
        # add the tmp_environment to a set to ensure no collisions
        @@tmp_environment_set ||= Set.new
        deadman = 100; loop_num = 0
        while @@tmp_environment_set.include?(tmp_environment = environment.downcase + '_' + random_string) do
          break if (loop_num = loop_num + 1) > deadman
        end
        @@tmp_environment_set << tmp_environment
        tmpdir = File.join('','tmp',tmp_environment)

        on master, "mkdir -p #{tmpdir}/manifests #{tmpdir}/modules"

        # register teardown to remove the link below
        teardown do
          on master, "rm -rf #{File.join(environmentpath,tmp_environment)}"
        end

        # WARNING: this won't work with filesync (symlinked environments are not supported)
        on master, "ln -sf #{tmpdir} #{File.join(environmentpath,tmp_environment)}"
        return tmp_environment
      end
      module_function :mk_tmp_environment, :environmentpath

    end
  end
end
