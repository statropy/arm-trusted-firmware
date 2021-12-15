#!/bin/env ruby
# The BL1 binary is controlled by the BL2_AT_EL3 flag
# in plat/microchip/lan966x/lan966x_evb/platform.mk

require 'fileutils'
require 'optparse'
require 'pp'

build_platforms         = %I[lan966x_evb lan966x_sr lan966x_b0 lan969x_sr]
build_types             = %I[debug release]
build_authentifications = %I[auth ssk bssk]

build_variant_args      = { bl2normal: '', bl2noop: '--variant noop', bl2noop_otp: '--variant noop_otp' }
build_auth_args         = { auth: '',
                            ssk:  '--encrypt-ssk keys/ssk.bin --encrypt-images bl2,bl32,bl33',
                            bssk: '--encrypt-bssk keys/huk.bin --encrypt-images bl2,bl32,bl33' }

option = {}
OptionParser.new do |opts|
  opts.banner = %(Usage: #{__FILE__} [options]

  Build all combinations of TF-A artifacts

  Options are:)
  opts.on('-c', '--clean', 'Remove build artifacts') do
    option[:clean] = true
  end
end.order!

def build_variants(platform)
    v = []
    case platform
    when :lan966x_b0
        v = %I[bl2normal bl2noop bl2noop_otp]
    when :lan966x_evb, :lan966x_sr, :lan969x_sr
        v = %I[bl2normal]
    else
        raise "Unknown platform: #{platform}"
    end
    return v
end

def banner(artifacts, cmd)
  5.times { puts }
  puts '#' * 80
  artifacts.each { |_from, to| puts "#{'#' * 5} Artifact: #{to}" unless File.exist?(to) }
  puts "#{'#' * 5} Command: #{cmd}"
  puts '#' * 80
  5.times { puts }
end

def cleanup(do_exit = false)
  files = Dir.glob('*.bl1') + Dir.glob('*.bin') + Dir.glob('*.fip') + Dir.glob('*.img') + Dir.glob('*.gpt') + Dir.glob('*.dump')
  FileUtils.rm_f(files, verbose: true)
  FileUtils.rm_rf('build', verbose: true)
  FileUtils.rm_rf('artifacts', verbose: true)
  exit(0) if do_exit
end

def pre_build
  puts '=' * 80
  puts 'Pre build cleanup'
  cleanup
  puts '=' * 80
end

cleanup(true) if option[:clean]

pre_build
build_platforms.each do |bp|
  build_types.each do |bt|
    build_variants(bp).each do |bv|
      build_authentifications.each do |ba|
        dst = "#{bp}-#{bt}-#{bv}-#{ba}"
        artifacts = [
          ["build/#{bp}/#{bt}/fip.bin",      "#{dst}.fip"],
          ["build/#{bp}/#{bt}/fip.gpt",      "#{dst}.gpt"],
          ["build/#{bp}/#{bt}/#{bp}.img",    "#{dst}.img"],
          ["build/#{bp}/#{bt}/bl1/bl1.dump", "#{dst}-bl1.dump"],
          ["build/#{bp}/#{bt}/bl2/bl2.dump", "#{dst}-bl2.dump"]
        ]
        # Limit the BL1 image artifacts
        dst = "#{bp}-#{bt}"
        if bp == :lan966x_sr && bv == :bl2normal && ba == :auth
          artifacts << ["build/#{bp}/#{bt}/bl1.bin",      "#{dst}.bl1"]
        elsif bp == :lan966x_b0 && bt == :release && bv == :bl2normal && ba == :auth
          artifacts << ["build/#{bp}/#{bt}/bl1.bin",      "#{dst}.bl1"]
        end
        cargs = "--#{bt} --gptimg --norimg #{build_auth_args[ba]} -p #{bp} #{build_variant_args[bv]}"
        cmd = "ruby scripts/build.rb #{cargs}"
        cmd_clean = 'ruby scripts/build.rb distclean'
        banner(artifacts, cmd)
        system(cmd_clean)
        system(cmd)
        artifacts.each do |from, to|
          to = "artifacts/#{bp}/#{to}"
          FileUtils.mkdir_p(File.dirname(to))
          FileUtils.mv(from, to, verbose: true) if !File.exist?(to) && File.exist?(from)
        end
      end
    end
  end
end
#  vim: set ts=4 sw=2 sts=2 tw=120 et cc=120 ft=ruby :
