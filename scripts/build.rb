#!/bin/env ruby

require 'fileutils'
require 'optparse'
require 'yaml'
require 'digest'
require 'pp'

platforms = {
    "lan966x_sr"	=> Hash[
        :uboot =>  "u-boot-lan966x_sr_atf.bin",
        :bl2_at_el3 => false ],
    "lan966x_a0"	=> Hash[
        :uboot =>  "u-boot-lan966x_evb_atf.bin",
        :bl2_at_el3 => true  ],
    "lan966x_b0"	=> Hash[
        :uboot =>  "u-boot-lan966x_evb_atf.bin",
        :bl2_at_el3 => false ],
}

bssk_derive_key = [
		0x80, 0x66, 0xae, 0x0a, 0x98, 0x8c, 0xf1, 0x64,
		0x8c, 0x55, 0x76, 0x02, 0xd3, 0xe7, 0x9e, 0x92,
		0x2c, 0x37, 0x00, 0x7f, 0xd6, 0x43, 0x9d, 0x16,
		0x94, 0xdd, 0x46, 0x2a, 0xcc, 0x61, 0xb5, 0x5d,
]

$option = { :platform              => "lan966x_b0",
            :loglevel              => 40,
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
            :sdk                   => "2022.09",
#            :sdk_branch            => "-brsdk",
            :norimg                => true,
            :gptimg                => false,
            :ramusage              => true,
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
    opts.on("-x", "--variant X", "BL2 variant (noop)") do |v|
        $option[:bl2variant] = v
    end
    opts.on("-l", "--linux-as-bl33", "Enable direct Linux booting") do
        $option[:linux_boot] = true
    end
    opts.on("--bl33-blob <file>", "BL33 binary") do |p|
        $option[:bl33_blob] = p
    end
    opts.on("-d", "--debug", "Enable DEBUG") do
        $option[:debug] = true
    end
    opts.on("--release", "Disable DEBUG") do
        $option[:debug] = false
    end
    opts.on("-n", "--[no-]norimg", "Create a NOR image file with the FIP (lan966x_a0 only)") do |v|
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
    brsdk_name = "mscc-brsdk-#{$option[:arch]}-#{$option[:sdk]}"
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
    $tc_bin = "/opt/mscc/#{tc_path}/arm-cortex_a8-linux-gnueabihf/bin"
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
    ENV["CROSS_COMPILE"] = "#{$tc_bin}/arm-linux-"
    puts "Using toolchain #{tc_vers} at #{$tc_bin}"
end

pdef = platforms[$option[:platform]]
raise "Unknown platform: #{$option[:platform]}" unless pdef

if $option[:debug]
    args += "DEBUG=1 "
    build = "build/#{$option[:platform]}/debug"
else
    args += "DEBUG=0 "
    build = "build/#{$option[:platform]}/release"
end
FileUtils.mkdir_p build

sdk_dir = install_sdk()
tc_conf = YAML::load( File.open( sdk_dir + "/.mscc-version" ) )
install_toolchain(tc_conf["toolchain"])

# Use SDK tools first in PATH
ENV['PATH'] = "#{sdk_dir}/arm-cortex_a8-linux-gnu/standalone/release/x86_64-linux/bin:" + ENV['PATH']

if $option[:bl33_blob]
    args += "BL33=#{$option[:bl33_blob]} "
elsif $option[:linux_boot]
    kernel = sdk_dir + "/arm-cortex_a8-linux-gnu/standalone/release/brsdk_standalone_arm.itb"
    args += "BL33=#{kernel} "
else
    uboot = sdk_dir + "/arm-cortex_a8-linux-gnu/bootloaders/lan966x/" + pdef[:uboot]
    args += "BL33=#{uboot} "
end

args += "PLAT=#{$option[:platform]} ARCH=aarch32 AARCH32_SP=sp_min "
args += "BL2_VARIANT=#{$option[:bl2variant].upcase} " if $option[:bl2variant]
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
do_cmd "git -C mbedtls checkout -q 2aff17b8c55ed460a549db89cdf685c700676ff7"

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
    exit 0 if $option[:clean]
    $cov_dir = "cov_" + $option[:platform]
    FileUtils.rm_rf $cov_dir
    FileUtils.mkdir $cov_dir
end

args += "LOG_LEVEL=#{$option[:loglevel]} " if $option[:loglevel]

if $option[:create_keys]
    targets = "certificates"
else
    if ARGV.length > 0
        targets = ARGV.join(" ")
    else
        targets = "all fip fwu_fip"
    end
end

cmd = "make #{args} #{targets}"
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

lsargs = %w(bin gz)

# produce GZIP FIP
fip = "#{build}/fip.bin"
if File.exist?(fip)
    do_cmd("gzip -c #{fip} > #{fip}.gz")
end

if $option[:norimg] && pdef[:bl2_at_el3]
    img = build + "/" + $option[:platform] + ".img"
    # BL2 placed in the start of FLASH
    b = "#{build}/bl2.bin"
    FileUtils.cp(b, img)
    tsize = 80
    do_cmd("truncate --size=#{tsize}k #{img}")
    # Reserve UBoot env 2 * 256k
    tsize += 512
    do_cmd("truncate --size=#{tsize}k #{img}")
    # Lastly, the FIP
    do_cmd("cat #{fip} >> #{img}")
    # List binaries
    lsargs << "img"
end

if $option[:gptimg]
    gptfile = "#{build}/fip.gpt"
    begin
        # Get size of FIP
        size = File.size?(fip)
        # Convert size to sectors
        fip_blocks = (size / 512.0).ceil();
        # Align partitions to multiple of 2048
        fip_blocks = (fip_blocks / 2048.0).ceil() * 2048;
        # Reserve first 2048 blocks for partition table
        main_partsize = 2048
        # reserve last 64 blocks for backup partition table
        back_partsize = 64
        total_blocks = (fip_blocks * 2) + main_partsize + back_partsize
	# Add env partition, 1MB
	env_blocks = (1024 * 1024) / 512
	total_blocks += env_blocks
        if $option[:linux_boot]
            # 256M root
            root_blocks = (256 * 1024 * 1024) / 512
            total_blocks += root_blocks
        else
	    # Add linux partition, 32MB
	    linux_blocks = (32 * 1024 * 1024) / 512
	    total_blocks += linux_blocks
	    # Add linux bk partition, 32MB
	    linux_bk_blocks = (32 * 1024 * 1024) / 512
	    total_blocks += linux_bk_blocks
	    # Add data partition, 32MB
	    data_blocks = (32 * 1024 * 1024) / 512
	    total_blocks += data_blocks
        end
        # Create partition file of appropriate size
        do_cmd("dd if=/dev/zero of=#{gptfile} bs=512 count=#{total_blocks}")
        do_cmd("parted -s #{gptfile} mktable gpt")
        # Add first main partition
        p_start = main_partsize
        p_end = p_start + fip_blocks -1
        do_cmd("parted -s #{gptfile} mkpart fip #{p_start}s #{p_end}s")
        # Inject data
        do_cmd("dd status=none if=#{fip} of=#{gptfile} seek=#{p_start} bs=512 conv=notrunc")
        # Add second backup partition
        p_start += fip_blocks
        p_end += fip_blocks
        do_cmd("parted -s #{gptfile} mkpart fip.bak #{p_start}s #{p_end}s")
        # Inject data
        do_cmd("dd status=none if=#{fip} of=#{gptfile} seek=#{p_start} bs=512 conv=notrunc")
        # Add U-Boot environment partition
        p_start = p_end + 1
        p_end += env_blocks
        do_cmd("parted -s #{gptfile} mkpart Env #{p_start}s #{p_end}s")
        # Add Linux partition
        if $option[:linux_boot]
            # Add root partition
            p_start = p_end + 1
            p_end += root_blocks
            do_cmd("parted -s #{gptfile} mkpart root #{p_start}s #{p_end}s")
            # Inject data
            root = sdk_dir + "/arm-cortex_a8-linux-gnu/standalone/release/rootfs.ext4"
            do_cmd("dd status=none if=#{root} of=#{gptfile} seek=#{p_start} bs=512 conv=notrunc")
        else
            # Add linux partiton
            p_start = p_end + 1
            p_end += linux_blocks
            do_cmd("parted -s #{gptfile} mkpart Boot0 #{p_start}s #{p_end}s")
            # Add linux backup partition
            p_start = p_end + 1
            p_end += linux_bk_blocks
            do_cmd("parted -s #{gptfile} mkpart Boot1 #{p_start}s #{p_end}s")
            # Add data partition
            p_start = p_end + 1
            p_end += data_blocks
            do_cmd("parted -s #{gptfile} mkpart Data #{p_start}s #{p_end}s")
        end
    end
    do_cmd("gdisk -l #{gptfile}")
    do_cmd("gzip < #{gptfile} > #{gptfile}.gz")
    lsargs << "gpt"
end

# Build FWU
lsargs << "html"
do_cmd("ruby ./scripts/html_inline.rb ./scripts/fwu/serial.html > #{build}/fwu.html")

# List binaries
do_cmd("ls -l " + lsargs.map{|s| "#{build}/*.#{s}"}.join(" "))

if $option[:coverity]
    do_cmd("cov-analyze -dir #{$cov_dir} --jobs auto")
    do_cmd("cov-commit-defects -dir #{$cov_dir} --stream #{$option[:coverity]} --config /opt/coverity/credentials.xml --auth-key-file /opt/coverity/reporter.key")
end

if $option[:ramusage]
    usage = {}
    ["bl1", "bl2"].each do |s|
        elf = "#{build}/#{s}/#{s}.elf"
        if File.exist? elf
            o = `#{$tc_bin}/arm-linux-size #{elf}`
            raise "Unable to read size of #{elf} - $?" unless $?.success?
            b1 = o.split("\n")[1]
            d = b1.match(/(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+(\d+)/);
            d = d[1, 5]
            d = d.map { |d| d.to_i }
            usage[s] = d
        end
    end
    raise "No RAM usage report, no ELF data" if usage.length == 0
    raise "No bl1 data" if !usage['bl1'] && !pdef[:bl2_at_el3]
    raise "No bl2 data" if !usage['bl2']
    if pdef[:bl2_at_el3]
        d2 = usage['bl2']
        sram = d2[0] + d2[1] + d2[2]
        printf "BL2: %dK - %d bytes spare. Code %d, data %d, bss %d\n",
               sram / 1024, (128 * 1024) - sram, d2[0], d2[1], d2[2]
    else
        d1 = usage['bl1']
        d2 = usage['bl2']
        bl1 = d1[1] + d1[2]
        bl2 = d2[0] + d2[1] + d2[2]
        sram = bl1 + bl2
        rom = d1[0]
        printf "BL1: Code %d, data %d, bss %d\n", d1[0], d1[1], d1[2]
        printf "BL2: Code %d, data %d, bss %d\n", d2[0], d2[1], d2[2]
        printf "ROM: %dK - %d bytes spare\n", rom / 1024, (80 * 1024) - rom
        printf "SRAM: %dK - %d bytes spare\n", sram / 1024, (128 * 1024) - sram
    end
end

#  vim: set ts=8 sw=4 sts=4 tw=120 et cc=120 ft=ruby :
