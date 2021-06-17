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

/* OTP access functions */
#define otp_accessor_group_read(aname, grp_name)			\\
static inline int aname(uint8_t *dst, size_t nbytes)			\\
{									\\
	assert((nbytes * 8) >= grp_name##_LEN);				\\
	return otp_read_bits(dst, grp_name##_ADDR * 8, grp_name##_LEN); \\
}

#define otp_accessor_read_bool(aname, grp_name, fld_name)		\\
static inline bool aname(void)						\\
{									\\
	uint8_t b;							\\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\\
	int off = OTP_##fld_name##_OFF % 8;				\\
	(void) otp_read_bits(&b, addr * 8, 8);				\\
	return !!(b & (1 << off));					\\
}

#define otp_accessor_read_field(aname, grp_name, fld_name)		\\
static inline uint32_t aname(void)					\\
{									\\
	uint32_t w;							\\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\\
	int off = OTP_##fld_name##_OFF % 8;				\\
	(void) otp_read_uint32(&w, addr * 8);				\\
	return (w >> off) & GENMASK(OTP_##fld_name##_LEN - 1, 0);	\\
}

<%- for g in $data -%>
<%- if g["accessor"] != nil -%>
<%- gname = g["name"].downcase -%>
otp_accessor_group_read(otp_read_<%= gname %>, <%= g["name"] %>);
<%- end -%>
<%- for f in g["fields"] -%>
<%- if f["accessor"] != nil -%>
<%- fname = f["name"].downcase -%>
<%- if f["width"] == 1 -%>
otp_accessor_read_bool(otp_read_<%= fname %>, <%= g["name"] %>, <%= f["name"] %>);
<%- else -%>
otp_accessor_read_field(otp_read_<%= fname %>, <%= g["name"] %>, <%= f["name"] %>);
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

            if row[2] && row[2] != ""
                # puts "New group: #{row[1]}"
                if group
                    data << group
                    group = nil
                end
                group = Hash[
                    "accessor" => row[1],
                    "name"   => row[2].strip.upcase,
                    "width"  => row[4].to_i,
                    "address"   => row[8],
                    "desc"   => row[11],
                    "fields" => Array.new,
                ]
                ["address"].each do|k|
                    group[k].strip! if group[k]
                end
            else
                elem = Hash[
                    "accessor" => row[1],
                    "name"    => row[3].strip.upcase,
                    "width"   => row[5].to_i,
                    "desc"    => row[11],
                ]
                if elem["width"] > 1
                    ignore_exception { elem["offset"] = row[10].strip }
                else
                    elem["offset"] = row[10].to_i
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
        allblank = true
        g["fields"].each do |fld|
            if fld["offset"].is_a?(String)
                bits = fld["offset"].split(":")
                fld["offset"] = bits[1].to_i
            end
            allblank = allblank && (fld["offset"] == nil || fld["offset"] == 0)
        end
        # Assign bits iff all fields unassigned
        if allblank
            start = 0
            g["fields"].reverse_each do |fld|
                fld["offset"] = start
                start += fld["width"]
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
