#!/bin/env ruby

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

#include <assert.h>
#include <stdint.h>
#include <drivers/microchip/otp.h>

<%- for g in $data -%>
/* <%= g["name"] %> */
#define <%= sprintf("%s_%s", g["name"], "ADDR").ljust(40) %> <%= g["address"] %>
#define <%= sprintf("%s_%s", g["name"], "SIZE").ljust(40) %> <%= g["size"] %>

<%- if g["fields"].length > 0 -%>
/* Fields in <%= g["name"] %> */
<%- for f in g["fields"] -%>
#define <%= sprintf("%s_%s_%s", "OTP", f["name"], "OFF").ljust(40) %> <%= f["offset"] %>
<%- if f["width"] > 1 -%>
#define <%= sprintf("%s_%s_%s", "OTP", f["name"], "BITS").ljust(40) %> <%= f["width"] %>
<%- end -%>
<%- end -%>

<%- end -%>
<%- end -%>

/* OTP access functions */
#define otp_accessor_group_read(aname, grp_name)			\\
static inline int aname(uint8_t *dst, size_t nbytes)			\\
{									\\
	assert(nbytes >= grp_name##_SIZE);				\\
	return otp_read_bytes(grp_name##_ADDR, grp_name##_SIZE, dst);   \\
}

#define otp_accessor_read_bool(aname, grp_name, fld_name)		\\
static inline bool aname(void)						\\
{									\\
	uint8_t b;							\\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\\
	int off = OTP_##fld_name##_OFF % 8;				\\
	(void) otp_read_bytes(addr, 1, &b);				\\
	return !!(b & (1 << off));					\\
}

#define otp_accessor_read_bytes(aname, grp_name, fld_name)		\\
static inline int aname(uint8_t *dst, size_t nbytes)			\\
{									\\
	assert(nbytes >= (OTP_##fld_name##_BITS / 8));			\\
	assert((OTP_##fld_name##_OFF % 8) == 0);			\\
	return otp_read_bytes(grp_name##_ADDR + 			\\
		OTP_##fld_name##_OFF / 8, OTP_##fld_name##_BITS / 8, dst); \\
}

#define otp_accessor_read_field(aname, grp_name, fld_name)		\\
static inline uint32_t aname(void)					\\
{									\\
	uint32_t w;							\\
	int addr = grp_name##_ADDR + (OTP_##fld_name##_OFF / 8);	\\
	int off = OTP_##fld_name##_OFF % 8;				\\
	(void) otp_read_uint32(addr, &w);				\\
	return (w >> off) & GENMASK(OTP_##fld_name##_BITS - 1, 0);	\\
}

<%- for g in $data -%>
<%- if g["accessor"] -%>
<%- gname = g["name"].downcase -%>
otp_accessor_group_read(otp_read_<%= gname %>, <%= g["name"] %>);
<%- end -%>
<%- for f in g["fields"] -%>
<%- if f["accessor"] -%>
<%- fname = f["name"].downcase -%>
<%- if f["width"] == 1 -%>
otp_accessor_read_bool(otp_read_<%= fname %>, <%= g["name"] %>, <%= f["name"] %>);
<%- elsif (f["offset"] % 8) == 0 && (f["width"] % 8) == 0 -%>
otp_accessor_read_bytes(otp_read_<%= fname %>, <%= g["name"] %>, <%= f["name"] %>);
<%- else -%>
otp_accessor_read_field(otp_read_<%= fname %>, <%= g["name"] %>, <%= f["name"] %>);
<%- end -%>
<%- end -%>
<%- end -%>
<%- end -%>

#endif	/* PLAT_OTP_H */
)

def process_yaml(fn)
    return YAML::load( File.open( fn ) )
end

def output_yaml(fd)
    $data.to_yaml.each_line do |line|
        fd.puts line.rstrip
    end
end

def output_headers(fd)
    fd.puts ERB.new($template, nil, '-').result(binding)
end

def cleanup_addr(a)
    if a && a.is_a?(String)
        a.gsub!(/_/, "")
        a = a.hex
    end
    return a
end

def cleanup()
    offset = 0
    $data.each do|g|
        g["address"] = cleanup_addr(g["address"])
        raise "No size (#{g["name"]})" if g["size"] == nil || g["size"] == 0
        if g["address"] != nil && g["address"] != 0
            if g["address"] < offset
                raise "expected offset 0x%x for group %s, have 0x%x" % [
                  offset, g["name"], g["address"]
                ]
            end
        else
            g["address"] = offset
        end
        offset = g["size"] + g["address"]
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

def dump_table fd
  fd.printf "    Start    End     Size  RTL  EMU  AREA  Name\n"
  fd.printf "    ------   ------  ----  ---  ---  ----  ----------------\n"

  offset = 0
  $data.each do |g|
    star = " "

    if g["address"] > offset
      fd.printf "    reserved %d bytes\n" % [g["address"] - offset]
    end
    offset = g["address"] + g["size"]

    if g["size"] == 2 and g["address"] % 2 != 0
      star = "*"
    elsif g["size"] == 8 and g["address"] % 8 != 0
      star = "*"
    elsif g["size"] >= 4 and g["address"] % 4 != 0
      star = "*"
    end

    rtl = ""
    rtl = g["init_hw"] if g["init_hw"]

    emu = ""
    emu = "X" if g["emu"]

    fd.printf "    0x%04x%s  0x%04x  %4d  %3s  %3s  %4d  %-25s\n" %
        [g["address"], star, g["address"] + g["size"] - 1, g["size"], rtl, emu, g["area"], g["name"]]

  end
end

$options = {}
OptionParser.new do |opts|
    opts.banner = "Usage: otpgen.rb [options]"
    opts.version = 0.1
    opts.on("-y", "--read-yaml <file>", "Read YAML data") do |f|
        $data = process_yaml(f)
    end
    opts.on("-g", "--generate-headers <outfile>", "") do |f|
        $options[:headers] = File.open(f, "w")
    end

    opts.on("-Y", "--write-yaml <outfile>", "Write YAML output") do |f|
        $options[:yaml_out] = File.open(f, "w")
    end
end.order!

cleanup()

dump_table($stdout);

if $options[:headers]
    output_headers($options[:headers])
end

if $options[:yaml_out]
    output_yaml($options[:out])
end




