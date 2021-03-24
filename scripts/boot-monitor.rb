#!/bin/env ruby

require 'pp'
require 'io/console'
require 'digest/crc32'

CMD_SOF  = '>'
CMD_ACK  = 'A'
CMD_QUIT = 'Q'
CMD_VERS = 'V'
CMD_SEND = 'S'
CMD_DATA = 'D'
CMD_EXEC = 'E'

$id = 0

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
               :err => items[1].hex,
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
                while rsp[:err] == 0
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
        ns = ARGV.shift.to_i
        STDOUT.write fmt_req(CMD_EXEC,ns)
        rsp = read_resp(STDIN)
    when "quit"
        STDOUT.write fmt_req(CMD_QUIT)
        rsp = read_resp(STDIN)
    else
        STDERR.puts "Invalid command: #{arg}"
        exit
    end
end
