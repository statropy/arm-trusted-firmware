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
/* <%= g["name"] %> */
#define <%= sprintf("%s_%s", g["name"], "ADDR").ljust(40) %> <%= g["address"] %>
#define <%= sprintf("%s_%s", g["name"], "LEN").ljust(40) %> <%= g["width"] %>

<%- if g["fields"].length > 0 -%>
/* Fields in <%= g["name"] %> */
<%- for f in g["fields"] -%>
#define <%= sprintf("%s_%s_%s", "OTP", f["name"], "OFF").ljust(40) %> <%= f["offset"] %>
<%- if f["width"] > 1 -%>
#define <%= sprintf("%s_%s_%s", "OTP", f["name"], "LEN").ljust(40) %> <%= f["width"] %>
<%- end -%>
<%- end -%>

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
                    "name"   => row[1].strip.upcase,
                    "width"  => row[3].to_i,
                    "address"   => row[7],
                    "desc"   => row[10],
                    "fields" => Array.new,
                ]
                ["address"].each do|k|
                    group[k].strip! if group[k]
                end
            else
                elem = Hash[
                    "name"    => row[2].strip.upcase,
                    "width"   => row[4].to_i,
                    "desc"    => row[10],
                ]
                if elem["width"] > 1
                    ignore_exception { elem["offset"] = row[9].strip }
                else
                    elem["offset"] = row[9].to_i
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

def cleanup_addr(a)
    if a
        a.gsub!(/_/, "")
        a = a.hex
    end
    return a
end

def cleanup()
    offset = 0
    $data.each do|g|
        g["address"] = cleanup_addr(g["address"])
        raise "No width" if g["width"] == nil || g["width"] == 0
        if g["address"] != nil && g["address"] != 0
            raise "expected offset #{offset} for group #{g["name"]}, have #{g["address"]}" if g["address"] < offset
        else
            g["address"] = offset
        end
        offset = (g["width"] / 8) + g["address"]
        g["fields"].each do |fld|
            if fld["offset"].is_a?(String)
                bits = fld["offset"].split(":")
                fld["offset"] = bits[1].to_i
            end
        end
    end
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

cleanup()

if $options[:headers]
    output_headers($options[:out])
else
    output_yaml($options[:out])
end
