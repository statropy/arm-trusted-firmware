#!/bin/env ruby

require 'optparse'
require 'digest'
require 'pp'

$options = { }
OptionParser.new do |opts|
    opts.banner = "Usage: sjtag-challenge.rb [options] <hex-words>"
    opts.version = 0.1
    opts.on("-i", "--input <file>", "Read challenge data from file") do |f|
        $options[:input] = f;
    end
    opts.on("-k", "--key <file>", "Read shared key data from file (mandatory)") do |f|
        $options[:key] = File.binread(f);
        raise "Key data must be 32 bytes" unless $options[:key].length == 32
    end
    opts.on("-h", "--hexdigest", "Output as hex digest (for boot monitor ruby script)") do
        $options[:hexdigest] = true;
    end
end.order!

raise "Missing key option, use --key <file>" if $options[:key].nil?

data = nil
if $options[:input]
    data = File.binread($options[:input])
else
    data = ARGV.map(&:hex).pack("V*")
end

raise "No challenge data provided" unless data
raise "Must have 32 bytes of data" unless data.length == 32
raise "Must have 32 bytes key data" unless $options[:key].length == 32

# Pre-shared SJTAG key is appended to data
data += $options[:key]

# The challenge response is a SHA256 hash of (challenge + sjtag-key)
if $options[:hexdigest]
    response = Digest::SHA256.hexdigest(data)
else
    response = Digest::SHA256.digest(data).unpack("V*").map{|i| i.to_s(16)}.join(" ")
end
puts "Response: #{response}"
