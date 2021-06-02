#!/bin/env ruby

require 'pp'
require 'digest/crc32'

$options = {}
$options[:update] = true

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
