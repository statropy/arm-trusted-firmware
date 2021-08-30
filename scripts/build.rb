#!/bin/env ruby

require 'fileutils'
require 'optparse'
require 'yaml'
require 'pp'

platforms = {
    "lan966x_sr"	=> Hash[ :uboot =>  "u-boot-lan966x_sr_atf.bin",  :bl2_at_el3 => false ],
    "lan966x_evb"	=> Hash[ :uboot =>  "u-boot-lan966x_evb_atf.bin", :bl2_at_el3 => true  ],
    "lan966x_b0"	=> Hash[ :uboot =>  "u-boot-lan966x_evb_atf.bin", :bl2_at_el3 => false ],
}

$option = { :platform	=> "lan966x_sr",
             :loglevel	=> 40,
             :tbbr	=> false,
             :debug	=> true,
             :rot	=> "keys/rotprivk_rsa.pem",
             :arch	=> "arm",
             :sdk	=> "2021.02-483",
             :norimg	=> true,
             :gptimg	=> false,
          }

args = ""

OptionParser.new do |opts|
    opts.banner = "Usage: build.rb [options]"
    opts.version = 0.1
    opts.on("-p", "--platform <platform>", "Build for given platform") do |p|
        $option[:platform] = p
    end
    opts.on("-r", "--root-of-trust <keyfile>", "Set ROT key file") do |k|
        $option[:rot] = k
    end
    opts.on("-t", "--[no-]tbbr", "Enable/disable TBBR") do |v|
        $option[:tbbr] = v
    end
    opts.on("-x", "--variant X", "BL2 variant (noop)") do |v|
        $option[:bl2variant] = v
    end
    opts.on("-d", "--[no-]debug", "Enable/disable DEBUG") do |v|
        $option[:debug] = v
    end
    opts.on("-g", "--[no-]norimg", "Create a NOR image file with the FIP") do |v|
        $option[:norimg] = v
    end
    opts.on("-g", "--[no-]gptimg", "Create a GPT image file with the FIP") do |v|
        $option[:gptimg] = v
    end
    opts.on("-c", "--clean", "Do a 'make clean' instead of normal build") do |v|
        $option[:clean] = true
    end
    opts.on("-C", "--coverity stream", "Enable coverity scan") do |cov|
        $option[:coverity] = cov
    end
end.order!

def do_cmd(cmd)
    system(cmd)
    if !$?.success?
        raise("\"#{cmd}\" failed.")
    end
end

def do_cmdret(cmd)
    ret = `#{cmd}`
    if !$?.success?
        raise("\"#{cmd}\" failed.")
    end
    ret
end

def install_sdk()
    brsdk_name = "mscc-brsdk-#{$option[:arch]}-#{$option[:sdk]}"
    brsdk_base = "/opt/mscc/#{brsdk_name}"
    if not File.exist? brsdk_base
        if File.exist? "/usr/local/bin/mscc-install-pkg"
            do_cmd "sudo /usr/local/bin/mscc-install-pkg -t brsdk/#{$option[:sdk]}-brsdk #{brsdk_name}"
        end
        raise "Unable to install SDK to #{brsdk_base}" unless File.exist? brsdk_base
    end
    return brsdk_base
end

def install_toolchain(tc_vers)
    tc_folder = tc_vers
    tc_folder = "#{tc_vers}-toolchain" if not tc_vers.include? "toolchain"
    tc_path = "mscc-toolchain-bin-#{tc_vers}"
    bin = "/opt/mscc/" + tc_path + "/arm-cortex_a8-linux-gnueabihf/bin"
    if !File.directory?(bin)
        do_cmd "sudo /usr/local/bin/mscc-install-pkg -t toolchains/#{tc_folder} #{tc_path}"
        raise "Unable to install toolchain to #{bin}" unless File.exist?(bin)
    end
    ENV["CROSS_COMPILE"] = "#{bin}/arm-linux-"
    puts "Using toolchain #{tc_vers} at #{bin}"
end

pdef = platforms[$option[:platform]]
raise "Unknown platform: #{$option[:platform]}" unless pdef

sdk_dir = install_sdk()
tc_conf = YAML::load( File.open( sdk_dir + "/.mscc-version" ) )
install_toolchain(tc_conf["toolchain"])

bootloaders = sdk_dir + "/arm-cortex_a8-linux-gnu/bootloaders/lan966x"
uboot = bootloaders + "/" + pdef[:uboot]
args += "BL33=#{uboot} "

args += "PLAT=#{$option[:platform]} ARCH=aarch32 AARCH32_SP=sp_min "
args += "BL2_VARIANT=#{$option[:bl2variant].upcase} " if $option[:bl2variant]

if $option[:tbbr]
    args += "TRUSTED_BOARD_BOOT=1 GENERATE_COT=1 CREATE_KEYS=1 ROT_KEY=#{$option[:rot]} MBEDTLS_DIR=mbedtls "
    if !File.directory?("mbedtls")
        do_cmd("git clone https://github.com/ARMmbed/mbedtls.git")
    end
    # We're currently using this as a reference - needs to be in sync with TFA
    do_cmd "git -C mbedtls checkout -q 2aff17b8c55ed460a549db89cdf685c700676ff7"
end

if $option[:debug]
    args += "DEBUG=1 "
    build = "build/#{$option[:platform]}/debug"
else
    args += "DEBUG=0 "
    build = "build/#{$option[:platform]}/release"
end

if $option[:clean] || $option[:coverity]
    puts "Cleaning " + build
    FileUtils.rm_rf build
    exit 0 if $option[:clean]
    $cov_dir = "cov_" + $option[:platform]
    FileUtils.rm_rf $cov_dir
    FileUtils.mkdir $cov_dir
end

args += "LOG_LEVEL=#{$option[:loglevel]} " if $option[:loglevel]

if ARGV.length > 0
    targets = ARGV.join(" ")
else
    targets = "all fip"
end

cmd = "make #{args} #{targets}"
cmd = "cov-build --dir #{$cov_dir} #{cmd}" if $option[:coverity]
puts cmd
do_cmd cmd

img = build + "/" + $option[:platform] + ".img"
if pdef[:bl2_at_el3]
    # BL2 placed in the start of FLASH
    b = "#{build}/bl2.bin"
else
    # BL2 is in FIP
    b = "/dev/null"
end

exit(0) if ARGV.length == 1 && (ARGV[0] == 'distclean' || ARGV[0] == 'clean')

if $option[:norimg]
    FileUtils.cp(b, img)
    tsize = 80
    do_cmd("truncate --size=#{tsize}k #{img}")
    # Reserve UBoot env 2 * 256k
    tsize += 512
    do_cmd("truncate --size=#{tsize}k #{img}")
    do_cmd("cat #{build}/fip.bin >> #{img}")
    # List binaries
    do_cmd("ls -l #{build}/*.bin #{build}/*.img")
end

if $option[:gptimg]
    gptfile = "#{build}/fip.gpt"
    bkupgpt = "#{build}/backup.gpt"
    FileUtils.rm_f(gptfile)
    begin
        do_cmd("dd if=/dev/zero of=#{gptfile} bs=512 count=6178")
        do_cmd("parted -s #{gptfile} mktable gpt")
        do_cmd("parted -s #{gptfile} mkpart fip 2048s 4095s")           # Align partitions to multipla of 1024
        do_cmd("parted -s #{gptfile} mkpart fip.bak 4096s 6143s")
        do_cmd("dd if=#{gptfile} of=#{bkupgpt} skip=6145 bs=512")       # Copy backup partition table
        do_cmd("dd if=#{build}/fip.bin of=#{gptfile} seek=2048 bs=512") # Insert fip in first partition
        do_cmd("dd if=#{bkupgpt} of=#{gptfile} seek=6145 bs=512")       # Restore backup partition table
    ensure
        FileUtils.rm_f(bkupgpt)
    end
    do_cmd("fdisk -l #{gptfile}")
end

if $option[:coverity]
    do_cmd("cov-analyze -dir #{$cov_dir} --jobs auto")
    do_cmd("cov-commit-defects -dir #{$cov_dir} --stream #{$option[:coverity]} --config /opt/coverity/credentials.xml --auth-key-file /opt/coverity/reporter.key")
end
#  vim: set ts=8 sw=4 sts=4 tw=120 et cc=120 ft=ruby :
