#!/bin/env ruby

require 'fileutils'
require 'optparse'
require 'pp'

build_platforms         = %I[lan966x_b0 lan966x_lm lan969x_sr lan969x_a0]
build_types             = %I[debug release]
build_authentifications = %I[auth ssk]
build_variants          = %I[bl2normal bl2noop bl33linux]

build_variant_args      = { bl2normal: '', bl2noop: '--variant noop', bl2noop_otp: '--variant noop_otp',
                            bl33linux: '--linux-as-bl33' }
build_auth_args         = { auth: '',
                            ssk:  '--encrypt-ssk keys/ssk.bin --encrypt-images bl2,bl31,bl32,bl33',
                            bssk: '--encrypt-bssk keys/huk.bin --encrypt-images bl2,bl31,bl32,bl33' }

option = {}
OptionParser.new do |opts|
  opts.banner = %(Usage: #{__FILE__} [options]

  Build all combinations of TF-A artifacts

  Options are:)
  opts.on('-c', '--clean', 'Remove build artifacts') do
    option[:clean] = true
  end
end.order!

def banner(artifacts, cmd)
  5.times { puts }
  puts '#' * 80
  artifacts.each { |_from, to| puts "#{'#' * 5} Artifact: #{to}" unless File.exist?(to) }
  puts "#{'#' * 5} Command: #{cmd}"
  puts '#' * 80
  5.times { puts }
end

def cleanup(do_exit = false)
  files = Dir.glob('*.bl1') + Dir.glob('*.bin') + Dir.glob('*.bl1.hex') + Dir.glob('*.fip') + Dir.glob('*.img') + Dir.glob('*.gpt') + Dir.glob('*.dump')
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
    build_variants.each do |bv|
      build_authentifications.each do |ba|
        dst = "#{bp}-#{bt}"
        # Loose stupid 'bl2normal' name
        if !bv.match("bl2normal")
          dst = "#{bv}/#{dst}-#{bv}"
        end
        # Loose stupid 'auth' name
        if !ba.match("auth")
          dst = "#{dst}-#{ba}"
        end
        artifacts = [
          ["build/#{bp}/#{bt}/fip.bin",      "#{dst}.fip"],
          ["build/#{bp}/#{bt}/fip.bin.gz",   "#{dst}.fip.gz"],
          ["build/#{bp}/#{bt}/mmc.gpt",      "#{dst}-mmc.gpt"],
          ["build/#{bp}/#{bt}/mmc.gpt.gz",   "#{dst}-mmc.gpt.gz"],
          ["build/#{bp}/#{bt}/nor.gpt",      "#{dst}-nor.gpt"],
          ["build/#{bp}/#{bt}/nor.gpt.gz",   "#{dst}-nor.gpt.gz"],
          ["build/#{bp}/#{bt}/#{bp}.img",    "#{dst}.img"],
          ["build/#{bp}/#{bt}/bl1.bin",      "#{dst}.bl1"],
          ["build/#{bp}/#{bt}/fwu.html",     "fwu-#{dst}.html"],
          ["build/#{bp}/#{bt}/bl1/bl1.dump", "#{dst}-bl1.dump"],
          ["build/#{bp}/#{bt}/bl2/bl2.dump", "#{dst}-bl2.dump"]
        ]
        cargs = "--#{bt} #{build_auth_args[ba]} -p #{bp} #{build_variant_args[bv]}"
        cmd = "ruby scripts/build.rb #{cargs}"
        cmd_clean = 'ruby scripts/build.rb distclean'
        banner(artifacts, cmd)
        system(cmd_clean)
        system(cmd)
        FileUtils.mkdir_p("artifacts")
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
