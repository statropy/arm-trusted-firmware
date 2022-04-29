#!/bin/env ruby

require 'pp'
require 'base64'

def inline_file(f)
    puts "<script>\n"
    File.readlines(f).each do |line|
        puts line
    end
    puts "</script>\n"
end

def inline_pic(f)
    ext = File.extname(f).strip.downcase[1..-1]
    data = "data:image/#{ext};base64,#{Base64.encode64(IO.binread(f))}"
end

# Check for input file argument
if ARGV.length != 1 || !File.readable?(ARGV[0])
  puts %(Usage: #{File.basename(__FILE__)} html_input)
  exit(1)
end

infilepath = File.expand_path(ARGV[0])
inp_dir = File.dirname(infilepath)

File.readlines(infilepath).each do |line|
    if (f = line.match(/<script src="([^"]+)"><\/script>/))
        inline_file(inp_dir + "/" + f[1])
    elsif line.match(/<img/) && (f = line.match(/src="([^"]+)"/))
        data = inline_pic(inp_dir + "/" + f[1])
        puts line.gsub(f[0], "src=\"#{data}\"")
    else
        puts(line)
    end
end

exit(0)
