#!/bin/env ruby

require 'base64'
require 'optparse'

$option = {
    :platform              => "lan966x_b0",
}

OptionParser.new do |opts|
    opts.banner = "Usage: build.rb [options]"
    opts.version = 0.1
    opts.on("-p", "--platform <platform>", "Build for given platform") do |p|
        $option[:platform] = p
    end
    opts.on("-o", "--output <platform>", "Output file") do |o|
        $option[:out] = o
    end
end.order!

File.open($option[:out], "w") { |f|
    f.write("const bl2u_platform = \"#{$option[:platform]}\";\n");
    f.write("const bl2u_app = [\n");
    ARGV.each do |fwu_fip|
        Base64.encode64(File.read(fwu_fip)).each_line { |l| l.chomp!; f.write("\t\"#{l}\",\n") }
    end
    f.write("]\n");
}
