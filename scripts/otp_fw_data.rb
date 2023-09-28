#!/usr/bin/env ruby

# This script can read a YAML file containing FW data for configuration of various devices (SPI, eMMC etc)
# and produce a binary file that can be included in the FW_CONFIG section of a FIP.

require 'fileutils'
require 'yaml'

FW_CONFIG_MAX_DATA = 128

# Check for both file arguments
if ARGV.length < 2
  puts %(Usage: #{File.basename(__FILE__)} yaml_data_file binary_outfile)
  exit(1)
end

infilepath = File.expand_path(ARGV[0])
outfilepath = File.expand_path(ARGV[1])

# Check that the YAML file is present
unless File.exist?(infilepath)
  puts %(Could not open YAML file)
  exit(1)
end

# Load yaml file, parse it and write a bitstream file with the data
class FirmwareBitstream
  def initialize(infile, outfile)
    @infile = infile
    @outfile = outfile
    @bytes = []
    @values = []
    @format = ''
  end

  def parse
    puts "Parsing #{@infile}"
    YAML.safe_load(File.open(@infile)).each do |item|
      store(item)
    end
    stream_out
  end

  def store(item)
    puts "* Storing #{item['field']} = #{item['value']}"
    @values << item['value']
    case item['size']
    when 32
      @format << 'L'
    when 16
      @format << 'S'
    when 8
      @format << 'C'
    end
  end

  def stream_out
    # Pad with null bytes
    @format << 'x' + FW_CONFIG_MAX_DATA.to_s
    # Truncate to FW_CONFIG_MAX_DATA and pack into bytearray
    @bytes = @values.pack(@format)[0..FW_CONFIG_MAX_DATA - 1]
  end

  def write
    puts "Writing #{@outfile}"
    File.open(@outfile, 'wb').write(@bytes)
  end
end

fwstream = FirmwareBitstream.new(infilepath, outfilepath)
fwstream.parse
fwstream.write
