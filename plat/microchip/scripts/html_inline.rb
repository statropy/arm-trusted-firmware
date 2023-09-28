#!/bin/env ruby

require 'pp'
require 'optparse'
require 'base64'

def inline_file(tag, f)
    puts "<#{tag}>\n"
    File.readlines(f).each do |line|
        puts line
    end
    puts "</#{tag}>\n"
end

def inline_pic(f)
    ext = File.extname(f).strip.downcase[1..-1]
    data = "data:image/#{ext};base64,#{Base64.encode64(IO.binread(f))}"
end

$option = { :incdir => nil, }

OptionParser.new do |opts|
    opts.banner = "Usage: $0 [options]"
    opts.version = 0.1
    opts.on("-i", "--include <dir>", "Use include dir") do |d|
        $option[:incdir] = d
    end
end.order!

# Check for input file argument
if ARGV.length != 1 || !File.readable?(ARGV[0])
  puts %(Usage: #{File.basename(__FILE__)} html_input)
  exit(1)
end

infilepath = File.expand_path(ARGV[0])
inp_dir = File.dirname(infilepath)

File.readlines(infilepath).each do |line|
    if (f = line.match(/<script src="([^"]+)"><\/script>/))
        if $option[:incdir]
            ifile = $option[:incdir] + "/" + f[1]
            ifile = inp_dir + "/" + f[1] if !File.exist?(ifile)
            inline_file("script", ifile)
        else
            ifile = inp_dir + "/" + f[1]
            inline_file("script", ifile)
        end
    elsif line.match(/<link/) && (f = line.match(/href="([^"]+)"/))
        ifile = inp_dir + "/" + f[1]
        inline_file("style", ifile)
    elsif line.match(/<img/) && (f = line.match(/src="([^"]+)"/))
        data = inline_pic(inp_dir + "/" + f[1])
        puts line.gsub(f[0], "src=\"#{data}\"")
    else
        puts(line)
    end
end

exit(0)
