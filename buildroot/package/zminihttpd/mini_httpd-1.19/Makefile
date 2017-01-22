# Makefile for mini_httpd

# CONFIGURE: If you are using a SystemV-based operating system, such as
# Solaris, you will need to uncomment this definition.
#SYSV_LIBS =    -lnsl -lsocket

# CONFIGURE: Some systems don't need -lcrypt, and indeed they get an
# error if you try to link with it.  If you get an error about libcrypt
# not found, try commenting out this definition.
CRYPT_LIB =    -lcrypt

# CONFIGURE: If you want to compile in support for https, uncomment these
# definitions.  You will need to have already built OpenSSL, available at
# http://www.openssl.org/  Make sure the SSL_TREE definition points to the
# tree with your OpenSSL installation - depending on how you installed it,
# it may be in /usr/local instead of /usr/local/ssl.
#SSL_TREE =	/usr/local/ssl
#SSL_DEFS =	-DUSE_SSL
#SSL_INC =	-I${SSL_TREE}/include
#SSL_LIBS =	-L /usr/lib/libssl.so.0.9.7 -L /usr/lib/libcrypto.so.0.9.7

SYSLIBS+= -L$(TOOLCHAIN_DIR)/lib/ -L$(BUILD_DIR)/staging_dir/lib/


# Install directories
INSTALL_DIR= $(PREFIX)/sbin/
INSTALL_INC= $(PREFIX)/../../include

# CONFIGURE: If you want to compile in support for PHP environment variables
# (namely PHP_AUTH_USER and PHP_AUTH_PW) then uncomment the definition below.
#PHP_ENV = 	-DPHP_ENV

# CONFIGURE: If you want to omit all IPV6 Support, uncomment the def below.
#IPV6_DEFS =	-DNO_IPV6

BINDIR =	/usr/local/sbin
MANDIR =	/usr/local/man
CDEFS =		${SSL_DEFS} ${SSL_INC} ${IPV6_DEFS} ${PHP_ENV}
CFLAGS =	-O ${CDEFS}
#CFLAGS =	-g ${CDEFS}
LDFLAGS =	-s
#LDFLAGS =	-g
LDLIBS =	${SSL_LIBS} ${SYSV_LIBS} ${CRYPT_LIB}


EXTRA_WARNINGS= -Wall -Wshadow -Werror
CFLAGS += -I$(TOOLCHAIN_DIR)/include/ -I$(BUILD_DIR)/staging_dir/include/ -I$(BUILD_DIR)/../../include/
CFLAGS += $(TARGET_CFLAGS) $(EXTRA_WARNINGS)

#CFLAGS += -DSUPPORT_DATA_MODEL

CROSS_COMPLE=$(TOOLCHAIN_DIR)/../bin/arc-linux-uclibc-
CC=$(CROSS_COMPLE)gcc
LD=$(CROSS_COMPLE)ld
AR=$(CROSS_COMPLE)ar
LDLIBS+= $(SYSLIBS)

all:		mini_httpd htpasswd

mini_httpd:	mini_httpd.o match.o tdate_parse.o
	${CC} ${CFLAGS} ${LDFLAGS} mini_httpd.o match.o tdate_parse.o ${LDLIBS} -o mini_httpd

mini_httpd.o:	mini_httpd.c version.h port.h match.h tdate_parse.h mime_encodings.h mime_types.h
	${CC} ${CFLAGS} -c mini_httpd.c

match.o:	match.c match.h
	${CC} ${CFLAGS} -c match.c

tdate_parse.o:	tdate_parse.c tdate_parse.h
	${CC} ${CFLAGS} -c tdate_parse.c

mime_encodings.h: mime_encodings.txt
	rm -f mime_encodings.h
	sed < mime_encodings.txt > mime_encodings.h \
	  -e 's/#.*//' -e 's/[ 	]*$$//' -e '/^$$/d' \
	  -e 's/[ 	][ 	]*/", 0, "/' -e 's/^/{ "/' -e 's/$$/", 0 },/'

mime_types.h: mime_types.txt
	rm -f mime_types.h
	sed < mime_types.txt > mime_types.h \
	  -e 's/#.*//' -e 's/[ 	]*$$//' -e '/^$$/d' \
	  -e 's/[ 	][ 	]*/", 0, "/' -e 's/^/{ "/' -e 's/$$/", 0 },/'


htpasswd:	htpasswd.o
	${CC} ${CFLAGS} ${LDFLAGS} htpasswd.o ${CRYPT_LIB} -o htpasswd

htpasswd.o:	htpasswd.c
	${CC} ${CFLAGS} -c htpasswd.c


cert:		mini_httpd.pem
mini_httpd.pem:	mini_httpd.cnf
	openssl req -new -x509 -days 365 -nodes -config mini_httpd.cnf -out mini_httpd.pem -keyout mini_httpd.pem
	openssl x509 -subject -dates -fingerprint -noout -in mini_httpd.pem
	chmod 600 mini_httpd.pem


install:	all
	install -m 755 -d $(INSTALL_DIR)
	install -m 755 htpasswd $(INSTALL_DIR)
	install -m 755 mini_httpd $(INSTALL_DIR)
	cp -f ./minihttpd.conf $(PREFIX)/etc/
	cp -f ./minihttpd.pem $(PREFIX)/etc/

clean:
	rm -f mini_httpd mime_encodings.h mime_types.h htpasswd mini_httpd.rnd *.o core core.* *.core

tar:
	@name=`sed -n -e '/SERVER_SOFTWARE/!d' -e 's,.*mini_httpd/,mini_httpd-,' -e 's, .*,,p' version.h` ; \
	  rm -rf $$name ; \
	  mkdir $$name ; \
	  tar cf - `cat FILES` | ( cd $$name ; tar xfBp - ) ; \
	  chmod 644 $$name/Makefile $$name/mime_encodings.txt $$name/mime_types.txt ; \
	  chmod 755 $$name/contrib $$name/contrib/redhat-rpm ; \
	  tar cf $$name.tar $$name ; \
	  rm -rf $$name ; \
	  gzip $$name.tar
