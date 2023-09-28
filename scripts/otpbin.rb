#!/bin/env ruby

require 'yaml'
require 'optparse'
require 'pp'

$bits = [ 0 ] * (1024)
$data = nil
$startoff = 256
$maxlen   = 384

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
    off = field["offset"] / 8
    $bits[off, elem["data"].size] = elem["data"]
    if (off < $startoff || off > ($startoff + $maxlen))
        raise "#{field["name"]}: Only data from #{$startoff} and #{$maxlen} fwd supported"
    end
end

def process_yaml(fn)
    return YAML::load( File.open( fn ) )
end

def find_elem(s, n)
    s.each do|g|
        if g["name"] == n
            return Hash[ "name" => n,"offset" => g["address"] * 8, "width" => g["size"] * 8]
        end
        g["fields"].each do|f|
            if f["name"] == n
                return Hash[ "name" => n, "offset" => g["address"] * 8 + f["offset"], "width" => f["width"]]
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

if $data
    $data.each do |e|
      f = find_elem(schema, e["field"])
      raise "Unknown OTP field #{e["field"]}" unless f
      set_bits(e, f)
    end
end

# output
bitstream = $bits.pack("C*")
bitstream = bitstream[$startoff, $maxlen]
$options[:out].write(bitstream);
