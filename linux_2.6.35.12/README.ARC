How to build the ARC linux kernel to a known working configuration.

1. Provide kernel build system with a suitable config file.
make ARCH=arc defconfig     # selects arch/arc/configs/aa4_defconfig

2. To tweak the .config
make ARCH=arc  menuconfig  # there's a new cool looking nconfig target as well

3. Build it (no need to use the bootpImage target now)
make ARCH=arc

Notes:

1. ARCH=arc for each cmd can be an inconvenince. You can either add it to
    your top level Makefile (locally) or define an env var or define a shell
    alias mymake='make ARCH=arc' etc

2. kernel headers are needed for building the uClibc based ARC GNU
    tool-chain. However passing it a virgin checked-out kernel tree will trip
    the tool-chain build, as some of the uClibc sources #include generated
    kernel headers such as version.h and autoconf.h
    So do an intial kernel build - even if it fails because there is no
    elf32 tool-chain yet as it's sufficient to generate the needed hdrs.
