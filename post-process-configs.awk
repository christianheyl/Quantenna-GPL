#
# Copyright (C) 2008 Quantenna Communications Inc
# Released under the terms of the GNU GPL v2.0
#
# This AWK script takes the global configuration file produced by the
# top-level 'make [x]config' and splits it into its constituent parts.
# The individual files created are called ${NEW_CONFIG} and are saved
# in the directory indicated by the #prefix... lines embedded in the
# master config file.
#
# Variables defined without a directory (normally in the top-level
# Kconfig script after a 'source "@@"' command) are considered to be
# globally important and are copied to all generated config files.
#

BEGIN { 
	if (!("NEW_CONFIG" in ENVIRON)) {
		print "NEW_CONFIG not defined"
		exit 1
	}
	FS="#"
	files = 0
	conffile = ENVIRON["NEW_CONFIG"]
}

$2 ~ /prefix[0-9]+/ {
	outfile = ($3 conffile)
	getline
	if (outfile != conffile) {
		print $0 > outfile
		conffiles[outfile] = 1
	} else {
		toplabels = (toplabels $0 "\n")
	}
}

END {
	print toplabels > ".config.toplabels"
}
