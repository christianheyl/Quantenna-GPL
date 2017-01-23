
--------------------
General information:
--------------------

The content of this repository corresponds to the content of O2_GPL_acR4311.rar file, which is provided within the HomeBox 2 6441 (1.01.30) GPL-Release by Arcadyan (Astoria).

Out-of-the-box this content is unusable! Following main points were found:

- missing (external) toolchain!
- HELL of permission errors (all files within the .rar archive have 0644 permission)
- missing symlinks
- missing files (in kernel-folder)

Before the initial commit most of the permission errors were fixed. It is not known if there are more (because I only diffed to an older Release of a Quantenna GPL Release (for Netgear R7500), with another target).


-----------------------------
Missing (external toolchain):
-----------------------------

To obtain (the pre-compiled, external) toolchain you can either inquire it from Arcadyan (Astoria), mail to opensource@arcadyan.com, or you can use one from another QTN-based/QTN-using device, like Netgear R7500 (URL: http://www.downloads.netgear.com/files/GPL/R7500-and_qtn_gpl_src_V1.0.0.94.zip) or ZyXEL VMG9823-B10A (you have to inquire it yourself)

For testing and compilation of this source code the external toolchain from Netgear's R7500 was used. You need the QEnvInstaller.bin file (rename it to Qenvinstaller.bin) from the Quantenna GPL tarball.


Important Note:

The toolchain (from  Netgear GPL Release) is a 32bit executable "file". To run it on a 64-bit multi-architecture Ubuntu system you need to add the i386 architecture and install 3 library packages. For Ubuntu 14.04 you have to execute the following commands:

1. sudo dpkg --add-architecture i386
2. sudo apt-get update
3. sudo apt-get install libc6:i386 libncurses5:i386 libstdc++6:i386

Beside this, you can use a native 32bit Ubuntu system.


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

For debug logs, while building, you can use 'make V=99'. After build is completed, all built files can be found in tftp-folder.


----------------------
How to build packages:
----------------------

The following procedure describes a build with additional packages. It is assumed that a "build-from-scratch" has been done prior to this!

1. cd quantenna-GPL
2. cd ./builtroot
3. make menuconfig

   select the packages you like to add and save

4. cd ..
5. make topaz_rgmii_config
6. make image

Important Note: Step 4 (leave buildroot-folder to root-folder) and 5 are mandatory. Without them the build won't be executed properly


-----------------
Current problems:
-----------------

When trying to build with additional packages, the build fails. This happen on 64bit and 32bit Ubunutu (14.04) systems! For example if `iptables` package is added, the following error message occurs

`32bit Ubuntu`

```bzcat /home/freetz/Quantenna-GPL/buildroot/dl/iptables-1.3.7.tar.bz2 | tar -C /home/freetz/Quantenna-GPL/buildroot/build_arc   -xf -
touch /home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7/.unpacked
# Allow patches.  Needed for openwrt for instance.
toolchain/patch-kernel.sh /home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7 package/iptables/ iptables\*.patch
#
/home/freetz/Quantenna-GPL/buildroot/toolchain_build_arc-linux-uclibc/bin/sed -i -e "s;\[ -f /usr/include/netinet/ip6.h \];grep -q '__UCLIBC_HAS_IPV6__ 1' \
		/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/include/bits/uClibc_config.h;" /home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7/Makefile
touch /home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7/.configured
PATH="/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/bin:/home/freetz/Quantenna-GPL/buildroot/toolchain_build_arc-linux-uclibc/bin:/usr/local/ARC/gcc//bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games" AR=/usr/local/ARC/gcc//bin/arc-linux-uclibc-ar AS=/usr/local/ARC/gcc//bin/arc-linux-uclibc-as LD=/usr/local/ARC/gcc//bin/arc-linux-uclibc-ld NM=/usr/local/ARC/gcc//bin/arc-linux-uclibc-nm CC=/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc GCC=/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc CXX=/usr/local/ARC/gcc//bin/arc-linux-uclibc-g++ CPP=/usr/local/ARC/gcc//bin/arc-linux-uclibc-cpp RANLIB=/usr/local/ARC/gcc//bin/arc-linux-uclibc-ranlib STRIP=/usr/local/ARC/gcc//bin/arc-linux-uclibc-strip OBJCOPY=/usr/local/ARC/gcc//bin/arc-linux-uclibc-objcopy CC_FOR_BUILD="gcc" PKG_CONFIG_SYSROOT=/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir PKG_CONFIG=/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/usr/bin/pkg-config CXX="" \
	make MAKE="make -j1" -C /home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7 \
		KERNEL_DIR=/home/freetz/Quantenna-GPL/buildroot/toolchain_build_arc-linux-uclibc/linux PREFIX=/usr \
		CC=/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc COPT_FLAGS="-Os -pipe  -I/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC"
grep: /home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/include/bits/uClibc_config.h: Datei oder Verzeichnis nicht gefunden
make[4]: Verzeichnis »/home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7« wird betreten
Making dependencies: please wait...
make[4]: Verzeichnis »/home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7« wird verlassen
grep: /home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/include/bits/uClibc_config.h: Datei oder Verzeichnis nicht gefunden
make[4]: Verzeichnis »/home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7« wird betreten
Unable to resolve dependency on linux/netfilter_ipv4/ip_nat_rule.h. Try 'make clean'.


    Please try `make KERNEL_DIR=path-to-correct-kernel'.


Unable to resolve dependency on linux/netfilter_ipv4/ip_conntrack.h. Try 'make clean'.


    Please try `make KERNEL_DIR=path-to-correct-kernel'.


Extensions found:
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -Os -pipe  -I/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC -Wall -Wunused -I/home/freetz/Quantenna-GPL/buildroot/toolchain_build_arc-linux-uclibc/linux/include -Iinclude/ -DIPTABLES_VERSION=\"1.3.7\"  -D_UNKNOWN_KERNEL_POINTER_SIZE -fPIC -o extensions/libipt_ah_sh.o -c extensions/libipt_ah.c
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -shared  -o extensions/libipt_ah.so extensions/libipt_ah_sh.o
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -Os -pipe  -I/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC -Wall -Wunused -I/home/freetz/Quantenna-GPL/buildroot/toolchain_build_arc-linux-uclibc/linux/include -Iinclude/ -DIPTABLES_VERSION=\"1.3.7\"  -D_UNKNOWN_KERNEL_POINTER_SIZE -fPIC -o extensions/libipt_addrtype_sh.o -c extensions/libipt_addrtype.c
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -shared  -o extensions/libipt_addrtype.so extensions/libipt_addrtype_sh.o
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -Os -pipe  -I/home/freetz/Quantenna-GPL/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC -Wall -Wunused -I/home/freetz/Quantenna-GPL/buildroot/toolchain_build_arc-linux-uclibc/linux/include -Iinclude/ -DIPTABLES_VERSION=\"1.3.7\"  -D_UNKNOWN_KERNEL_POINTER_SIZE -fPIC -o extensions/libipt_comment_sh.o -c extensions/libipt_comment.c
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -shared  -o extensions/libipt_comment.so extensions/libipt_comment_sh.o
make[4]: Verzeichnis »/home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7« wird verlassen
make[3]: *** [/home/freetz/Quantenna-GPL/buildroot/build_arc/iptables-1.3.7/iptables] Fehler 2
make[3]: Verzeichnis »/home/freetz/Quantenna-GPL/buildroot« wird verlassen
make[2]: *** [do_buildroot] Fehler 2
make[2]: Verzeichnis »/home/freetz/Quantenna-GPL« wird verlassen
make[1]: *** [buildroot] Fehler 2
make[1]: Verzeichnis »/home/freetz/Quantenna-GPL« wird verlassen
make: *** [image] Fehler 2
```


`64bit Ubuntu`

```bzcat /home/chris/test/buildroot/dl/iptables-1.3.7.tar.bz2 | tar -C /home/chris/test/buildroot/build_arc   -xf -
touch /home/chris/test/buildroot/build_arc/iptables-1.3.7/.unpacked
# Allow patches.  Needed for openwrt for instance.
toolchain/patch-kernel.sh /home/chris/test/buildroot/build_arc/iptables-1.3.7 package/iptables/ iptables\*.patch
#
/home/chris/test/buildroot/toolchain_build_arc-linux-uclibc/bin/sed -i -e "s;\[ -f /usr/include/netinet/ip6.h \];grep -q '__UCLIBC_HAS_IPV6__ 1' \
		/home/chris/test/buildroot/build_arc/staging_dir/include/bits/uClibc_config.h;" /home/chris/test/buildroot/build_arc/iptables-1.3.7/Makefile
touch /home/chris/test/buildroot/build_arc/iptables-1.3.7/.configured
PATH="/home/chris/test/buildroot/build_arc/staging_dir/bin:/home/chris/test/buildroot/toolchain_build_arc-linux-uclibc/bin:/usr/local/ARC/gcc//bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games" AR=/usr/local/ARC/gcc//bin/arc-linux-uclibc-ar AS=/usr/local/ARC/gcc//bin/arc-linux-uclibc-as LD=/usr/local/ARC/gcc//bin/arc-linux-uclibc-ld NM=/usr/local/ARC/gcc//bin/arc-linux-uclibc-nm CC=/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc GCC=/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc CXX=/usr/local/ARC/gcc//bin/arc-linux-uclibc-g++ CPP=/usr/local/ARC/gcc//bin/arc-linux-uclibc-cpp RANLIB=/usr/local/ARC/gcc//bin/arc-linux-uclibc-ranlib STRIP=/usr/local/ARC/gcc//bin/arc-linux-uclibc-strip OBJCOPY=/usr/local/ARC/gcc//bin/arc-linux-uclibc-objcopy CC_FOR_BUILD="gcc" PKG_CONFIG_SYSROOT=/home/chris/test/buildroot/build_arc/staging_dir PKG_CONFIG=/home/chris/test/buildroot/build_arc/staging_dir/usr/bin/pkg-config CXX="" \
	make MAKE="make -j1" -C /home/chris/test/buildroot/build_arc/iptables-1.3.7 \
		KERNEL_DIR=/home/chris/test/buildroot/toolchain_build_arc-linux-uclibc/linux PREFIX=/usr \
		CC=/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc COPT_FLAGS="-Os -pipe  -I/home/chris/test/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC"
grep: /home/chris/test/buildroot/build_arc/staging_dir/include/bits/uClibc_config.h: Datei oder Verzeichnis nicht gefunden
make[4]: Verzeichnis »/home/chris/test/buildroot/build_arc/iptables-1.3.7« wird betreten
Making dependencies: please wait...
make[4]: Verzeichnis »/home/chris/test/buildroot/build_arc/iptables-1.3.7« wird verlassen
grep: /home/chris/test/buildroot/build_arc/staging_dir/include/bits/uClibc_config.h: Datei oder Verzeichnis nicht gefunden
make[4]: Verzeichnis »/home/chris/test/buildroot/build_arc/iptables-1.3.7« wird betreten
Unable to resolve dependency on linux/netfilter_ipv4/ip_nat_rule.h. Try 'make clean'.


    Please try `make KERNEL_DIR=path-to-correct-kernel'.


Unable to resolve dependency on linux/netfilter_ipv4/ip_conntrack.h. Try 'make clean'.


    Please try `make KERNEL_DIR=path-to-correct-kernel'.


Extensions found:
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -Os -pipe  -I/home/chris/test/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC -Wall -Wunused -I/home/chris/test/buildroot/toolchain_build_arc-linux-uclibc/linux/include -Iinclude/ -DIPTABLES_VERSION=\"1.3.7\"  -D_UNKNOWN_KERNEL_POINTER_SIZE -fPIC -o extensions/libipt_ah_sh.o -c extensions/libipt_ah.c
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -shared  -o extensions/libipt_ah.so extensions/libipt_ah_sh.o
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -Os -pipe  -I/home/chris/test/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC -Wall -Wunused -I/home/chris/test/buildroot/toolchain_build_arc-linux-uclibc/linux/include -Iinclude/ -DIPTABLES_VERSION=\"1.3.7\"  -D_UNKNOWN_KERNEL_POINTER_SIZE -fPIC -o extensions/libipt_addrtype_sh.o -c extensions/libipt_addrtype.c
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -shared  -o extensions/libipt_addrtype.so extensions/libipt_addrtype_sh.o
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -Os -pipe  -I/home/chris/test/buildroot/build_arc/staging_dir/include -I/usr/local/ARC/gcc//arc-linux-uclibc/include -fPIC -fPIC -Wall -Wunused -I/home/chris/test/buildroot/toolchain_build_arc-linux-uclibc/linux/include -Iinclude/ -DIPTABLES_VERSION=\"1.3.7\"  -D_UNKNOWN_KERNEL_POINTER_SIZE -fPIC -o extensions/libipt_comment_sh.o -c extensions/libipt_comment.c
/usr/local/ARC/gcc//bin/arc-linux-uclibc-gcc -shared  -o extensions/libipt_comment.so extensions/libipt_comment_sh.o
make[4]: Verzeichnis »/home/chris/test/buildroot/build_arc/iptables-1.3.7« wird verlassen
make[3]: *** [/home/chris/test/buildroot/build_arc/iptables-1.3.7/iptables] Fehler 2
make[3]: Verzeichnis »/home/chris/test/buildroot« wird verlassen
make[2]: *** [do_buildroot] Fehler 2
make[2]: Verzeichnis »/home/chris/test« wird verlassen
make[1]: *** [.first_run] Fehler 2
make[1]: Verzeichnis »/home/chris/test« wird verlassen
make: *** [image] Fehler 2
```
