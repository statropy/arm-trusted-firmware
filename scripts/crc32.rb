#!/bin/env ruby

require 'pp'
require 'digest/crc32'
require 'optparse'

$options = {}
OptionParser.new do |opts|
    opts.banner = "Usage: crc32.rb [options] files..."
    opts.version = 0.1

    opts.on("-a", "--append", "Append crc bytes to input file (default)") do
        $options[:update] = false
    end
    opts.on("-u", "--update", "Update crc (last 4 bytes)") do
        $options[:update] = true
    end
end

ARGV.each do|f|
    data = File.read(f)
    if $options[:update]
        # Loose existing CRC
        data = data.slice(0..-5)
    end
    pp Digest::CRC32c.hexdigest(data)
    ck = Digest::CRC32c.checksum(data)
    #pp [ck].pack("V");
    File.open(f, "w") do|o|
        o.write(data)
        o.write([ck].pack("V"));
    end
end
