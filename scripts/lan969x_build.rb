#!/bin/env ruby

require 'fileutils'
require 'optparse'
require 'pp'

build_platforms         = %I[lan969x_a0]
build_types             = %I[release]
build_authentifications = %I[ssk]
build_auth_args         = { auth: '',
                            ssk:  '--encrypt-ssk keys/ssk.bin --encrypt-images bl2,bl31,bl32,bl33',
                            bssk: '--encrypt-bssk keys/huk.bin --encrypt-images bl2,bl31,bl32,bl33' }

$option = {}
OptionParser.new do |opts|
  opts.banner = %(Usage: #{__FILE__} [options]

  Build all combinations of TF-A artifacts

  Options are:)
  opts.on('-c', '--clean', 'Remove build artifacts') do
    $option[:clean] = true
  end
  opts.on('-d', '--dry-run', 'Dont actually do any opertations') do
    $option[:dry_run] = true
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
  return if $option[:dry_run]
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

cleanup(true) if $option[:clean]

pre_build
build_platforms.each do |bp|
  build_types.each do |bt|
    build_authentifications.each do |ba|
      dst = "#{bp}-#{bt}"
      # Loose stupid 'auth' name
      if !ba.match("auth")
        dst = "#{dst}-#{ba}"
      end
      artifacts = [
        ["build/#{bp}/#{bt}/fip.bin",      "#{dst}.fip"],
        ["build/#{bp}/#{bt}/fip.bin.gz",   "#{dst}.fip.gz"],
        ["build/#{bp}/#{bt}/mmc.gpt",      "#{dst}-mmc.gpt"],
        ["build/#{bp}/#{bt}/mmc.gpt.gz",   "#{dst}-mmc.gpt.gz"],
        ["build/#{bp}/#{bt}/fip_linux.bin",    "#{dst}-linux.fip"],
        ["build/#{bp}/#{bt}/fip_linux.bin.gz", "#{dst}-linux.fip.gz"],
        ["build/#{bp}/#{bt}/mmc-linux.gpt",    "#{dst}-mmc-linux.gpt"],
        ["build/#{bp}/#{bt}/mmc-linux.gpt.gz", "#{dst}-mmc-linux.gpt.gz"],
        ["build/#{bp}/#{bt}/nor.gpt",      "#{dst}-nor.gpt"],
        ["build/#{bp}/#{bt}/nor.gpt.gz",   "#{dst}-nor.gpt.gz"],
        ["build/#{bp}/#{bt}/#{bp}.img",    "#{dst}.img"],
      ]
      # Save cleartext FWU
      if ba.match("auth")
        artifacts << ["build/#{bp}/#{bt}/fwu.html",      "fwu-#{bp}-#{bt}.html"]
      end
      # Save cleartext+release BL1
      if ba.match("auth") && bt.match("release")
        artifacts << ["build/#{bp}/#{bt}/bl1.bin",       "#{dst}.bl1"]
        artifacts << ["build/#{bp}/#{bt}/fwu_fip.bin",   "#{dst}-fwu_fip.bin"]
      end
      cargs = "--#{bt} #{build_auth_args[ba]} -p #{bp}"
      cmd = "ruby scripts/build.rb #{cargs}"
      cmd_clean = 'ruby scripts/build.rb distclean'
      banner(artifacts, cmd)
      if $option[:dry_run]
        puts "*Not* running:"
        puts cmd_clean
        puts cmd
      else
        system(cmd_clean)
        system(cmd)
        FileUtils.mkdir_p("artifacts")
      end
      artifacts.each do |from, to|
        to = "artifacts/#{bp}/#{to}"
        if $option[:dry_run]
          FileUtils.mv(from, to, verbose: true, noop: true) if !File.exist?(to) && File.exist?(from)
        else
          FileUtils.mkdir_p(File.dirname(to))
          FileUtils.mv(from, to, verbose: true) if !File.exist?(to) && File.exist?(from)
        end
      end
    end
  end
end
#  vim: set ts=4 sw=2 sts=2 tw=120 et cc=120 ft=ruby :
