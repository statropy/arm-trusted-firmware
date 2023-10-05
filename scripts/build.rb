#!/bin/env ruby

require 'fileutils'
require 'optparse'
require 'yaml'
require 'digest'
require 'pp'
require 'base64'

platforms = {
    "lan966x_b0"	=> Hash[
        :uboot => "arm-cortex_a8-linux-gnu/bootloaders/lan966x/u-boot-mchp_lan966x_evb.bin",
        :loglevel => 30,
        :arch  => "arm" ],
    "lan966x_lm"	=> Hash[
        :uboot => "arm-cortex_a8-linux-gnu/bootloaders/lan966x/u-boot-mchp_lan966x_evb.bin",
        :loglevel => 10,
        :arch  => "arm" ],
    "lan969x_sr"	=> Hash[
        :uboot => "arm64-armv8_a-linux-gnu/bootloaders/lan969x/u-boot-mchp_lan969x.bin",
        :arch  => "arm64",
        :nor_gpt_size   => 2*(1024 * 1024) ],
    "lan969x_a0"	=> Hash[
        :uboot => "arm64-armv8_a-linux-gnu/bootloaders/release/u-boot-mchp_lan969x.bin",
        :loglevel => 30,
        :arch  => "arm64",
        :nor_gpt_size   => 2*(1024 * 1024) ],
    "lan969x_svb"	=> Hash[
        :uboot => "arm64-armv8_a-linux-gnu/bootloaders/release/u-boot-mchp_lan969x.bin",
        :loglevel => 30,
        :arch  => "arm64",
        :nor_gpt_size   => 2*(1024 * 1024) ],
}

UNIT_1K = 1024
UNIT_1M = UNIT_1K * UNIT_1K
UNIT_1G = UNIT_1K * UNIT_1M

# eMMC/GPT block size
BLOCK_SIZE = 512

# GPT alignment default = 2048 blocks. Note this is used also for the
# main and backup GPT table in order to ensure all partitions are
# aligned and sized to this magnitude.
GPT_ALIGN_BLOCKS = 2048

# Size of GPT tables reserved for partitions
GPT_TABLE_SIZE = 34

mmc_part = [
    { name: "fip",     size: 128 * UNIT_1M, },
    { name: "fip.bak", size: 128 * UNIT_1M, },
    { name: "Env",     size: 2 * UNIT_1M, },
    { name: "Env.bak", size: 2 * UNIT_1M, },
    { name: "Boot0",   size: UNIT_1G, 'type': "ext4", },
    { name: "Boot1",   size: UNIT_1G, 'type': "ext4", },
    # Last partition extend till end
    { name: "Data",    size: 0, 'type'	=> "ext4", },
]

nor_part = [
    { name: "fip",     size: 960 * UNIT_1K, },
    { name: "fip.bak", size: 960 * UNIT_1K, },
    # Last partition extend till end
    { name: "Env",     size: -0, },
]

# Assume 4G = 3.6GiB eMMC capacity
mmc_gpt_size = 3 * UNIT_1G + 648 * UNIT_1M

architectures = {
    "arm" => Hash[
        :bsp_arch => "arm",
        :tc_dir   => "arm-cortex_a8-linux-gnueabihf",
        :tc_prf   => "arm-cortex_a8-linux-gnueabihf",
        :linux	  => "/arm-cortex_a8-linux-gnu/standalone/release/",
        :rom_sz   => 80 * 1024,
        :sram_sz  => 128 * 1024,
        :atf_arch => "aarch32", ],
    "arm64" => Hash[
        :bsp_arch => "arm64",
        :tc_dir   => "arm64-armv8_a-linux-gnu",
        :tc_prf   => "aarch64-armv8_a-linux-gnu",
        :linux	  => "/arm64-armv8_a-linux-gnu/standalone/release/",
        :rom_sz   => 128 * 1024,
        :sram_sz  => 2 * 1024 * 1024,
        :atf_arch => "aarch64", ],
}

bssk_derive_key = [
		0x80, 0x66, 0xae, 0x0a, 0x98, 0x8c, 0xf1, 0x64,
		0x8c, 0x55, 0x76, 0x02, 0xd3, 0xe7, 0x9e, 0x92,
		0x2c, 0x37, 0x00, 0x7f, 0xd6, 0x43, 0x9d, 0x16,
		0x94, 0xdd, 0x46, 0x2a, 0xcc, 0x61, 0xb5, 0x5d,
]

$option = { :platform              => "lan966x_b0",
            :encrypt               => false,
            :debug                 => true,
            :rot                   => "keys/rotprivk_ecdsa.pem",
            :rot_pub               => "keys/rotpk_ecdsa.der",
            :rot_sha               => "keys/rotpk_ecdsa_sha256.bin",
            :bl31_key              => "keys/bl31_ecdsa.pem",
            :bl32_key              => "keys/bl32_ecdsa.pem",
            :bl33_key              => "keys/bl33_ecdsa.pem",
            :non_trusted_world_key => "keys/non_trusted_world_ecdsa.pem",
            :scp_bl2_key           => "keys/scp_bl2_ecdsa.pem",
            :trusted_world_key     => "keys/trusted_world_ecdsa.pem",
            :create_keys           => false,
            :bl33_blob             => nil,
            :arch                  => "arm",
            :sdk                   => "2023.09-1",
            #:sdk_branch            => "-brsdk",
            :norimg                => true,
            :gptimg                => false,
            :ramusage              => true,
          }

args = ""
bl33 = ""

OptionParser.new do |opts|
    opts.banner = "Usage: build.rb [options]"
    opts.version = 0.1
    opts.on("-p", "--platform <platform>", "Build for given platform") do |p|
        $option[:platform] = p
    end
    opts.on("-r", "--root-of-trust <keyfile>", "Set ROT key file") do |k|
        $option[:rot] = k
    end
    opts.on("--create_keys", "Create new keys") do
        $option[:create_keys] = true
    end
    opts.on("--bl31-key <keyfile>", "Set BL31 key") do |k|
        $option[:bl31_key] = k
    end
    opts.on("--bl32-key <keyfile>", "Set BL32 key") do |k|
        $option[:bl32_key] = k
    end
    opts.on("--bl33-key <keyfile>", "Set BL33 key") do |k|
        $option[:bl33_key] = k
    end
    opts.on("--non_trusted_world-key <keyfile>", "Set non_trusted_world key") do |k|
        $option[:non_trusted_world_key] = k
    end
    opts.on("--scp_bl2-key <keyfile>", "Set scp_bl2 key") do |k|
        $option[:scp_bl2_key] = k
    end
    opts.on("--trusted_world-key <keyfile>", "Set trusted_world key") do |k|
        $option[:trusted_world_key] = k
    end
    opts.on("--encrypt-images <imagelist>", "List of encrypted images, eg BL2,BL32,BL33") do |k|
        $option[:encrypt_images] = k
    end
    opts.on("--encrypt-ssk <keyfile>", "Enable encryption with SSK") do |k|
        $option[:encrypt_key] = k
        $option[:encrypt_flag] = 0 # SSK
    end
    opts.on("--encrypt-bssk <keyfile>", "Enable encryption with BSSK") do |k|
        $option[:encrypt_key] = k
        $option[:encrypt_flag] = 1 # BSSK
    end
    opts.on("--fw-nvctr <counter>", "Set Secure FW NV counter for FIP") do |c|
        $option[:nvctr] = c
    end
    opts.on("--ntfw-nvctr <counter>", "Set Non-trusted FW NV counter for FIP") do |c|
        $option[:nt_nvctr] = c
    end
    opts.on("--bl33_blob <file>", "BL33 binary") do |p|
        $option[:bl33_blob] = p
    end
    opts.on("-d", "--debug", "Enable DEBUG") do
        $option[:debug] = true
    end
    opts.on("--release", "Disable DEBUG") do
        $option[:debug] = false
    end
    opts.on("-g", "--[no-]gptimg", "Create a GPT image file with the FIP (obsoleted)") do |v|
        puts "Always creating GPT images"
    end
    opts.on("--gpt-data <file>", "Add GPT payload") do |f|
        $option[:gpt_data] = f
    end
    opts.on("-c", "--clean", "Do a 'make clean' instead of normal build") do |v|
        $option[:clean] = true
    end
    opts.on("-C", "--coverity stream", "Enable coverity scan") do |cov|
        $option[:coverity] = cov
    end
    opts.on("-R", "--[no-]ramusage", "Report RAM usage") do |v|
        $option[:ramusage] = v
    end
    opts.on("--extra1 <file>", "Add BL32 EXTRA1 image to FIP") do |v|
        $option[:bl32extra1] = v
    end
    opts.on("--extra2 <file>", "Add BL32 EXTRA2 image to FIP") do |v|
        $option[:bl32extra2] = v
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

def detect_internal_network
    %x(ping -q -c 1 herent-sw-pkg.microsemi.net > /dev/null 2>&1)
    return $?.exitstatus == 0 ? true : nil
end

def install_sdk()
    brsdk_name = "mscc-brsdk-#{$arch[:bsp_arch]}-#{$option[:sdk]}"
    brsdk_base = "/opt/mscc/#{brsdk_name}"
    if not File.exist?(brsdk_base)
        if File.exist?("/usr/local/bin/mscc-install-pkg") and detect_internal_network()
            do_cmd "sudo /usr/local/bin/mscc-install-pkg -t brsdk/#{$option[:sdk]}#{$option[:sdk_branch]} #{brsdk_name}"
        else
            puts "Please install the BSP: #{brsdk_name}.tar.gz into /opt/mscc/"
            puts ""
            puts "This may be done by using the following command:"
            puts "sudo sh -c \"mkdir -p /opt/mscc && wget -O- http://mscc-ent-open-source.s3-eu-west-1.amazonaws.com/public_root/bsp/#{brsdk_name}.tar.gz | tar -xz -C /opt/mscc/\""
            exit 1
        end
    end
    return brsdk_base
end

def install_toolchain(tc_vers)
    tc_folder = tc_vers
    tc_folder = "#{tc_vers}-toolchain" if not tc_vers.include? "toolchain"
    tc_path = "mscc-toolchain-bin-#{tc_vers}"
    $tc_bin = "/opt/mscc/" + tc_path + "/" + $arch[:tc_dir] + "/bin"
    if not File.directory?($tc_bin)
        if File.exist?("/usr/local/bin/mscc-install-pkg") and detect_internal_network()
            do_cmd "sudo /usr/local/bin/mscc-install-pkg -t toolchains/#{tc_folder} #{tc_path}"
        else
            puts "Please install the toolchain: #{tc_path}.tar.gz into /opt/mscc/"
            puts ""
            puts "This may be done by using the following command:"
            puts "sudo sh -c \"mkdir -p /opt/mscc && wget -O- http://mscc-ent-open-source.s3-eu-west-1.amazonaws.com/public_root/toolchain/#{tc_path}.tar.gz | tar -xz -C /opt/mscc/\""
            exit 1
        end
    end
    ENV["CROSS_COMPILE"] = $tc_bin + "/" + $arch[:tc_prf] + "-"
    puts "Using toolchain #{tc_vers} at #{$tc_bin}"
end

def align_block(nblocks, align)
    return (nblocks.fdiv(align).ceil()) * align
end

def make_gpt_image(parts, gptfile, part_data, dev_size, align_blocks, truncate_part)
    # Reserve primary and backup tables
    max_blocks = (dev_size / BLOCK_SIZE) - 2*align_block(GPT_TABLE_SIZE, align_blocks)
    # Iterate partitions and calculate sector-based size
    sum = 0
    parts.each do |p|
        size = p[:size]
        if (size % BLOCK_SIZE) != 0
            raise "Uneven partition #{p[:name]} at size #{size}"
        end
        if size == 0
            p[:blocks] = max_blocks - sum - align_blocks
        else
            # To blocks
            size /= BLOCK_SIZE
            # Align partitions to multiple of 2048
            size = (size / align_blocks.to_f).ceil() * align_blocks;
            p[:blocks] = size
        end
        sum += size
    end
    # Create partition file of appropriate size
    do_cmd("dd if=/dev/zero of=#{gptfile} conv=sparse bs=1M count=#{dev_size / UNIT_1M}")
    do_cmd("parted -s #{gptfile} mktable gpt")
    # Reserve first 34 blocks for primary partition table
    p_start = align_block(GPT_TABLE_SIZE, align_blocks)
    # Create each partition, fill some with data
    parts.each do |p|
        name = p[:name]
        type = p[:type] ? p[:type] : ""
        p_end = p_start +  p[:blocks] - 1
        do_cmd("parted -s #{gptfile} --align minimal mkpart #{name} #{type} #{p_start}s #{p_end}s")
        # Fill with data?
        if data = part_data.fetch(name, false)
            # Convert data size to sectors
            data_size = File.size?(data)
            data_blocks = align_block((data_size / BLOCK_SIZE.to_f).ceil(), align_blocks)
            p[:data_blocks] = data_blocks
            raise "#{name}: partion data (#{data_blocks}) exceeds capacity (#{p[:blocks]})" if data_blocks > p[:blocks]
            do_cmd("dd status=none if=#{data} of=#{gptfile} seek=#{p_start} bs=#{BLOCK_SIZE} conv=notrunc")
        end
        p[:start_offset] = p_start
        p_start = p_end + 1
    end
    # Truncate after the last data block of partition data
    if truncate_part
        cutoff = BLOCK_SIZE * (align_blocks + parts[truncate_part][:start_offset] + parts[truncate_part][:data_blocks])
        do_cmd("truncate -s #{cutoff} #{gptfile}")
    end
    do_cmd("gdisk -l #{gptfile}")
    do_cmd("gzip < #{gptfile} > #{gptfile}.gz")
end

pdef = platforms[$option[:platform]]
raise "Unknown platform: #{$option[:platform]}" unless pdef
$arch = architectures[pdef[:arch]]
raise "Unknown architecture: #{pdef[:arch]}" unless $arch

if $option[:debug]
    args += "DEBUG=1 "
    build = "build/#{$option[:platform]}/debug"
else
    args += "DEBUG=0 "
    build = "build/#{$option[:platform]}/release"
end
FileUtils.mkdir_p build

$sdk_dir = install_sdk()
tc_conf = YAML::load( File.open( $sdk_dir + "/.mscc-version" ) )
install_toolchain(tc_conf["toolchain"])

# Use SDK tools first in PATH
ENV['PATH'] = "#{$sdk_dir}/#{$arch[:linux]}/x86_64-linux/bin:" + ENV['PATH']

if $option[:bl33_blob]
    bl33 = $option[:bl33_blob]
else
    if pdef[:uboot]
        bl33 = $sdk_dir + "/" + pdef[:uboot]
    else
        bl33 = "bin/#{$option[:platform]}/u-boot.bin"
    end
end

args += "PLAT=#{$option[:platform]} ARCH=#{$arch[:atf_arch]} "
if $arch[:atf_arch] == "aarch32"
    args += " AARCH32_SP=sp_min "
end
args += "BL32_EXTRA1=#{$option[:bl32extra1]} " if $option[:bl32extra1]
args += "BL32_EXTRA2=#{$option[:bl32extra2]} " if $option[:bl32extra2]

# TBBR: Former option, now always on
args += "GENERATE_COT=1 MBEDTLS_DIR=mbedtls "
if $option[:create_keys]
    args += "CREATE_KEYS=1 SAVE_KEYS=1 "
end
args += "KEY_ALG=ecdsa "
args += "ROT_KEY=#{$option[:rot]} "
args += "BL31_KEY=#{$option[:bl31_key]} "
args += "BL32_KEY=#{$option[:bl32_key]} "
args += "BL33_KEY=#{$option[:bl33_key]} "
args += "NON_TRUSTED_WORLD_KEY=#{$option[:non_trusted_world_key]} "
args += "SCP_BL2_KEY=#{$option[:scp_bl2_key]} "
args += "TRUSTED_WORLD_KEY=#{$option[:trusted_world_key]} "
args += "TFW_NVCTR_VAL=#{$option[:nvctr]} " if $option[:nvctr]
args += "NTFW_NVCTR_VAL=#{$option[:nt_nvctr]} " if $option[:nt_nvctr]

if !File.directory?("mbedtls")
    do_cmd("git clone https://github.com/microchip-ung/mbedtls.git")
end
# We're currently using this as a reference - needs to be in sync with TFA
do_cmd "git -C mbedtls checkout -q mbedtls-2.28.0"

if $option[:encrypt_images] && $option[:encrypt_key]
    $option[:encrypt_images].split(',').each do |image|
        args += "ENCRYPT_#{image.upcase}=1 "
    end
    key = File.binread($option[:encrypt_key]);
    raise "Key data must be 32 bytes" unless key.length == 32
    if $option[:encrypt_flag] == 1
        # Key is HUK, derive to form HUK
        key = key + bssk_derive_key.pack("C*")
        key = Digest::SHA256.digest( key )
    end
    key = key.unpack("C*").map{|i| sprintf("%02X", i)}.join("")
    # Random Nonce
    nonce = (0..11).map{ sprintf("%02X", rand(255)) }.join("")
    args += "DECRYPTION_SUPPORT=1 FW_ENC_STATUS=#{$option[:encrypt_flag]} ENC_KEY=#{key} ENC_NONCE=#{nonce} "
end

if $option[:clean] || $option[:coverity]
    puts "Cleaning " + build
    FileUtils.rm_rf build
    # cert_create is picky about openssl version changes
    puts "Cleaning tools/cert_create"
    do_cmd "make -C tools/cert_create clean"
    exit 0 if $option[:clean]
    $cov_dir = "cov_" + $option[:platform]
    FileUtils.rm_rf $cov_dir
    FileUtils.mkdir $cov_dir
end

args += "LOG_LEVEL=#{pdef[:loglevel]} " if pdef[:loglevel]

if $option[:create_keys]
    targets = "certificates"
else
    if ARGV.length > 0
        targets = ARGV.join(" ")
    else
        targets = "all fip fwu_fip"
    end
end

# Normal build
cmd = "make #{args} BL33=#{bl33} #{targets}"
cmd = "cov-build --dir #{$cov_dir} #{cmd}" if $option[:coverity]
puts cmd
do_cmd cmd

exit(0) if ARGV.length == 1 && (ARGV[0] == 'distclean' || ARGV[0] == 'clean')

if $option[:create_keys]
    do_cmd "openssl ec -in #{$option[:rot]} -inform PEM -outform DER -pubout > #{$option[:rot_pub]}"
    do_cmd "openssl dgst -sha256 -binary #{$option[:rot_pub]} > #{$option[:rot_sha]}"
    do_cmd "dd if=/dev/random bs=1 count=32 of=keys/ssk.bin"
    do_cmd "dd if=/dev/random bs=1 count=32 of=keys/huk.bin"
    exit 0
end

# Create Linux boot FIP
if $arch[:linux]
    kernel = $sdk_dir + $arch[:linux] + "brsdk_standalone_#{pdef[:arch]}.itb"
    cmd = "make #{args} BL33=#{kernel} FIP_NAME=fip_linux.bin fip"
    puts cmd
    do_cmd cmd
end

lsargs = %w(bin gpt gz html)

# produce GZIP FIP
fip = "#{build}/fip.bin"
if File.exist?(fip)
    do_cmd("gzip -c #{fip} > #{fip}.gz")
end
# produce GZIP Linux FIP
fip_linux = "#{build}/fip_linux.bin"
if File.exist?(fip_linux)
    do_cmd("gzip -c #{fip_linux} > #{fip_linux}.gz")
end

if pdef[:nor_gpt_size]
    gptfile = "#{build}/nor.gpt"
    part_data = { 'fip' => fip, 'fip.bak' => fip }
    make_gpt_image(nor_part, "#{build}/nor.gpt", part_data, pdef[:nor_gpt_size], 4096 / BLOCK_SIZE, false)
end

# MMC GPT file - normal + linux
part_data = { 'fip' => fip }
make_gpt_image(mmc_part, "#{build}/mmc.gpt", part_data, mmc_gpt_size, GPT_ALIGN_BLOCKS, 0)

if File.exist?(fip_linux)
    part_data = { 'fip' => fip_linux, 'fip.bak' => fip_linux,
                  'Boot0' => $sdk_dir + $arch[:linux] + "rootfs.ext4" }
    make_gpt_image(mmc_part, "#{build}/mmc-linux.gpt", part_data, mmc_gpt_size, GPT_ALIGN_BLOCKS, 4)
end

# DT's
if File.exist? "#{build}/fdts/"
    ls_fdt = " #{build}/fdts/*.dtb"
else
    ls_fdt = ""
end

# List binaries
do_cmd("ls -l " + lsargs.map{|s| "#{build}/*.#{s}"}.join(" ") + ls_fdt)

if $option[:coverity]
    do_cmd("cov-analyze -dir #{$cov_dir} --jobs auto")
    do_cmd("cov-commit-defects -dir #{$cov_dir} --stream #{$option[:coverity]} --config /opt/coverity/credentials.xml --auth-key-file /opt/coverity/reporter.key")
end

if $option[:ramusage]
    usage = {}
    ["bl1", "bl2"].each do |s|
        elf = "#{build}/#{s}/#{s}.elf"
        if File.exist? elf
            o = `#{$tc_bin}/#{$arch[:tc_prf]}-size #{elf}`
            raise "Unable to read size of #{elf} - $?" unless $?.success?
            b1 = o.split("\n")[1]
            d = b1.match(/(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+(\d+)/);
            d = d[1, 5]
            d = d.map { |d| d.to_i }
            usage[s] = d
        end
    end
    raise "No RAM usage report, no ELF data" if usage.length == 0
    raise "No bl1 data" if !usage['bl1']
    raise "No bl2 data" if !usage['bl2']
    d1 = usage['bl1']
    d2 = usage['bl2']
    bl1 = d1[1] + d1[2]
    bl2 = d2[0] + d2[1] + d2[2]
    sram = bl1 + bl2
    rom = d1[0]
    printf "BL1: Code %d, data %d, bss %d\n", d1[0], d1[1], d1[2]
    printf "BL2: Code %d, data %d, bss %d\n", d2[0], d2[1], d2[2]
    printf "ROM: %dK - %d bytes spare\n", rom / 1024, $arch[:rom_sz] - rom
    printf "SRAM: %dK - %d bytes spare\n", sram / 1024, $arch[:sram_sz] - sram
end

#  vim: set ts=8 sw=4 sts=4 tw=120 et cc=120 ft=ruby :
