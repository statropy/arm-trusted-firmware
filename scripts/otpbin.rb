#!/bin/env ruby

require 'yaml'
require 'optparse'
require 'digest/crc32'
require 'pp'

$bits = [ 255 ] * (1024)
$data = nil

def set_bits(elem, field)
    if elem["file"]
        elem["data"] = File.binread(elem["file"]).unpack("C*")
    end
    if (field["width"] / 8) != elem["data"].size
        raise "Length mismatch: #{elem["field"]} field #{field["width"]} bits vs #{8 * elem["data"].size}"
    end
    if (field["offset"] % 8) != 0
        raise "Non-byte offset not supported: #{elem["field"]} field offset #{field["offset"]}"
    end
    $bits[field["offset"] / 8, elem["data"].size] = elem["data"]
end

def process_yaml(fn)
    return YAML::load( File.open( fn ) )
end

def find_elem(s, n)
    s.each do|g|
        if g["name"] == n
            return Hash[ "offset" => g["address"] * 8, "width" => g["width"]]
        end
        g["fields"].each do|f|
            if f["name"] == n
                return Hash[ "offset" => g["address"] * 8 + f["offset"], "width" => f["width"]]
            end
        end
    end
    return nil
end

$options = { :out => $stdout,
             :schema => "scripts/otp.yaml" }
OptionParser.new do |opts|
    opts.banner = "Usage: otpbin.rb [options]"
    opts.version = 0.1
    opts.on("-o", "--output <file>", "Output to file") do |f|
        $options[:out] = File.open(f, "w")
    end
    opts.on("-y", "--read-yaml <file>", "Read YAML data file") do |f|
        $data = process_yaml(f)
    end
    opts.on("-s", "--schema <file>", "Set YAML schema file") do |f|
        $options[:schema] = f
    end
end.order!

schema = process_yaml($options[:schema])

$data.each do |e|
    f = find_elem(schema, e["field"])
    raise "Unknown OTP field #{e["field"]}" unless f
    set_bits(e, f)
end

# output, with required CRC
bitstream = $bits.pack("C*")
$options[:out].write(bitstream);
ck = Digest::CRC32c.checksum(bitstream)
$options[:out].write([ck].pack("V"));
