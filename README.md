
--------------------
General information:
--------------------

The content of this repository corresponds to the content of O2_GPL_acR4311.rar file, which is provided within the HomeBox 2 6441 (1.01.30) GPL-Release by Arcadyan (Astoria Networks).

Out-of-the-box this content is unusable! Following main points were found:

- missing (external) toolchain!
- HELL of permission errors (all files within the .rar archive have 0644 permission)
- missing symlinks
- missing files (in kernel-folder)
- broken packages

Before the initial commit most of the permission errors got fixed. It isn't known, if there are more (I only diffed to an older release of Netgear's Quantenna-GPL (R7500), with another target).


-----------------------------
Missing (external toolchain):
-----------------------------

To obtain (the pre-compiled, external) toolchain you can either inquire it from Arcadyan (Astoria), mail to opensource@arcadyan.com, or you can use one from another QTN-based/QTN-using device, like Netgear R7500 (URL: http://www.downloads.netgear.com/files/GPL/R7500-and_qtn_gpl_src_V1.0.0.94.zip) or ZyXEL VMG9823-B10A (you have to inquire it yourself)

For testing and compilation of this source code the external toolchain from Netgear's R7500 was used. You need the QEnvInstaller.bin file (rename it to Qenvinstaller.bin) from their Quantenna GPL tarball.


Important Note:

The toolchain is 32bit. To use it on 64-bit multi-architecture OS, you need to add the i386 architecture and install 3 library packages. For Ubuntu 14.04 you have to execute the following commands:

1. sudo dpkg --add-architecture i386
2. sudo apt-get update
3. sudo apt-get install libc6:i386 libncurses5:i386 libstdc++6:i386

Aside of this, you can use a native 32bit (Ubuntu) OS.


----------------------
How-to install/compile:
----------------------

The following procedure describes a "build-from-scratch" with the allreay given "default" configuration (topaz_rgmii_config)

1. git clone https://github.com/christianheyl/Quantenna-GPL.git quantenna-GPL
2. cd quantenna-GPL
3. copy Qenvinstaller.bin to this folder
4. chmod a+x Qenvinstaller.bin
5. ./Qenvinstaller.bin
6. make topaz_rgmii_config
7. make fromscratch

Note: You might need to sudo (step 4 and 5) to install the toolchain. After installation it's located in '/usr/local/ARC/gcc/'

For debug logs, while building, you can use 'make V=99 fromscratch'. After build is completed, u-boot and kernel image files can be found within the tftp directory.


----------------------
How to build packages:
----------------------

The following procedure describes a build with additional packages. It is assumed that a "build-from-scratch" has been done (once) prior to this!

1. cd quantenna-GPL
2. cd ./builtroot
3. make menuconfig
  select the packages you like to add and save
4. cd \.\.
5. make image

For debug logs, while building, you can use 'make V=99 image'. After build is completed, u-boot and kernel image files can be found within the tftp directory.


-----------------
Current problems:
-----------------

The available packages are old! Most of them compile without a problem and some of them needed fixes. Some of them, despite being updated to a newer version don't compile at all. The follwing list of packages needs further attention:

| Package       | Problem       |
|:------------- |:-------------|
| argus-3.0.0.rc.34 | not specified |
| samba-3.3.4 | Problem with assembler code, maybe an ARC toolchain problem |
| strace-4.12 | Problem with variable/struct declaration. |
| ltrace-0.7.3 | not specified |
