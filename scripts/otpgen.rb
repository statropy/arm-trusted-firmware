#!/bin/env ruby

require 'creek'
require 'erb'
require 'yaml'
require 'optparse'

def ignore_exception
   begin
     yield
   rescue Exception
   end
end

$template = %Q(
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLAT_OTP_H
#define PLAT_OTP_H

#include <drivers/microchip/otp.h>

<%- for g in $data -%>
<%- if g["fields"].length > 0 -%>
/* Group <%= g["name"] %> - <%= g["width"] %> bits */
<%- for f in g["fields"] -%>
#define <%= f["name"] %> <%= f["width"] %>
<%- end -%>
<%- else -%>
/* <%= g["name"] %> */
#define <%= g["name"] %> <%= g["width"] %>
<%- end -%>

<%- end -%>

#endif	/* PLAT_OTP_H */
)

def process_excel(fn)
    rowct = 0

    workbook = Creek::Book.new fn
    worksheets = workbook.sheets

    if worksheets[0]
        worksheet = worksheets[0]
        data = Array.new
        group = nil

        worksheet.rows.each do |row|
            rowct += 1
            next if rowct == 1
            row = row.values
            next if row[0] != "x"

            if row[1] && row[1] != ""
                # puts "New group: #{row[1]}"
                if group
                    data << group
                    group = nil
                end
                group = Hash[
                    "name"   => row[1].strip,
                    "width"  => row[3].to_i,
                    "start"  => row[7],
                    "end"    => row[8],
                    "desc"   => row[10],
                    "fields" => Array.new,
                ]
                ["start", "end"].each do|k|
                    group[k].strip! if group[k]
                end
            else
                elem = Hash[
                    "name"    => row[2].strip,
                    "width"   => row[4].to_i,
                    "desc"   => row[10],
                ]
                if elem["width"] > 1
                    ignore_exception { elem["bits"] = row[9].strip }
                else
                    elem["bits"] = row[9].to_i
                end
                group["fields"] << elem
            end

        end

        # Add last group collected
        data << group if group

    end
    return data
end

def process_yaml(fn)
    return YAML::load( File.open( fn ) )
end

def output_yaml(fd)
    fd.puts $data.to_yaml
end

def output_headers(fd)
    fd.puts ERB.new($template, nil, '-').result(binding)
end

$options = { :out => $stdout }
OptionParser.new do |opts|
    opts.banner = "Usage: otpgen.rb [options]"
    opts.version = 0.1
    opts.on("-o", "--output <file>", "Output to file") do |f|
        $options[:out] = File.open(f, "w")
    end
    opts.on("-e", "--read-excel <file>", "Read Excel") do |f|
        $data = process_excel(f)
    end
    opts.on("-y", "--read-yaml <file>", "Read YAML data") do |f|
        $data = process_yaml(f)
    end
    opts.on("-g", "--generate-headers", "") do |f|
        $options[:headers] = true
    end
end.order!

if $options[:headers]
    output_headers($options[:out])
else
    output_yaml($options[:out])
end
