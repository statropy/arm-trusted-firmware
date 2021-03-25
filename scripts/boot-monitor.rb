#!/bin/env ruby

require 'pp'
require 'io/console'
require 'digest/crc32'

CMD_SOF  = '>'
CMD_VERS = 'V'
CMD_SEND = 'S'
CMD_DATA = 'D'
CMD_AUTH = 'U'
CMD_EXEC = 'E'
CMD_STRAP= 'O'
CMD_CONT = 'C'
CMD_ACK  = 'a'
CMD_NACK = 'n'

def read_resp(fd)
    buf = ""
    while (fd.read(1) != CMD_SOF)
    end
    while (c = fd.read(1))
        if c == '#'
            break
        end
        buf += c
    end
    items = buf.split(",")
    obj = nil
    if items.length == 3
        obj = {:cmd => items[0],
               :arg => items[1].hex,
               :len => items[2].hex }
        len = items[2].hex
        payload = fd.read(len*2)
        obj[:payload] = [payload].pack('H*')
        obj[:rawdata] = buf + "#" + payload
        obj[:crc] = fd.read(8).downcase
        obj[:calccrc] = Digest::CRC32c.hexdigest(obj[:rawdata])
    end
    STDERR.puts "RSP '#{buf}' => #{obj}"
    return obj
end

def fmt_req(cmd, arg = 0, payload = nil)
    buf = sprintf("%c", cmd)
    buf += sprintf(",%08x", arg)
    buf += sprintf(",%08x#", !payload.nil? ? payload.length : 0)
    if (!payload.nil?)
        buf += payload.unpack('H*').first
    end
    crc = Digest::CRC32c.hexdigest(buf)
    return sprintf("%c", CMD_SOF) + buf + crc
end

def do_cmd(cmd)
    STDOUT.write cmd
    STDERR.puts "REQ '#{cmd}'"
    return read_resp(STDIN)
end

def show_examples(title, pdus)
    puts title
    pdus.each_with_index do|p, ix|
        printf "%s: %s\n", (ix & 1) == 0 ? "REQ" : "RSP", p
    end
end

STDIN.sync = true
STDOUT.sync = true
STDERR.reopen("/var/tmp/bootstrap.txt", "w")

ARGV.each do |arg|
    arg = ARGV.shift
    case arg
    when "version"
        do_cmd(fmt_req(CMD_VERS))
    when "send"
        file = ARGV.shift
        sz = File.size?(file)
        if sz
            rsp = do_cmd(fmt_req(CMD_SEND,sz))
            if rsp[:cmd] != CMD_ACK
                break
            end
            off = 0
            open(file, "r") do |io|
                while rsp[:cmd] == CMD_ACK
                    data = io.read(128)
                    if data && data.length
                        rsp = do_cmd(fmt_req(CMD_DATA,off,data))
                        if rsp[:cmd] != CMD_ACK
                            break
                        end
                        off += data.length
                    else
                        break
                    end
                end
            end
        end
    when "execute"
        STDOUT.write fmt_req(CMD_EXEC)
        rsp = read_resp(STDIN)
    when "continue"
        STDOUT.write fmt_req(CMD_CONT)
        rsp = read_resp(STDIN)
    when "examples"
        show_examples("Get Version", [
                          fmt_req(CMD_VERS),
                          fmt_req(CMD_ACK,0,"Version 1.3 Manic Mantis")
                      ])
        show_examples("Download code", [
                          fmt_req(CMD_SEND,16),
                          fmt_req(CMD_ACK,),
                          fmt_req(CMD_DATA,0,"01284567"),
                          fmt_req(CMD_ACK,0),
                          fmt_req(CMD_DATA,8,"89abcdef"),
                          fmt_req(CMD_ACK,8)
                      ])
        show_examples("Authenticate", [
                          fmt_req(CMD_AUTH,0,"The quick brown fox jumps over the lazy dog"),
                          fmt_req(CMD_ACK)
                      ])
        show_examples("Execute", [
                          fmt_req(CMD_EXEC),
                          fmt_req(CMD_ACK)
                      ])
        show_examples("Override strapping", [
                          fmt_req(CMD_STRAP,1),
                          fmt_req(CMD_ACK)
                      ])
        show_examples("Continue Boot", [
                          fmt_req(CMD_CONT),
                          fmt_req(CMD_NACK)
                      ])
    else
        STDERR.puts "Invalid command: #{arg}"
        exit
    end
end
