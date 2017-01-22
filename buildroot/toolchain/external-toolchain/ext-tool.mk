#
# copy_toolchain_lib_root
#
# $1: source
# $2: destination
# $2: strip (y|n)	default is to strip
#
copy_toolchain_lib_root =									\
	LIB="$(strip $1)";									\
	DST="$(strip $2)";									\
	STRIP="$(strip $3)";									\
												\
	LIB_DIR=`$(TARGET_CC) -print-file-name=$${LIB} | sed -e "s,/$${LIB}\$$,,"`;		\
												\
	if test -z "$${LIB_DIR}"; then								\
		echo "copy_toolchain_lib_root: lib=$${LIB} not found";				\
		exit -1;									\
	fi;											\
												\
	LIB="$(strip $1)";									\
	for FILE in `cd $${LIB_DIR}; find . -type l -name "$${LIB}*" -maxdepth 3`; do		\
		LIB=$${FILE};									\
		LIB_DIRNAME=""; 								\
		while test \! -z "$${LIB}"; do							\
			LIB_BASENAME=`basename $${LIB}`;					\
			LIB_DIRNAME=$${LIB_DIRNAME}"`dirname $${LIB}`"/;			\
			LIB=$${LIB_DIRNAME}/$${LIB_BASENAME};					\
			LIB_DST=$(TARGET_DIR)$${DST}/$${LIB_DIRNAME}/; 				\
			echo "copy_toolchain_lib_root $${LIB} to $${LIB_DST}";			\
			rm -fr $${LIB_DST}/$${LIB_BASENAME};					\
			mkdir -p $${LIB_DST};							\
			if test -h $${LIB_DIR}/$${LIB}; then					\
				cp -d $${LIB_DIR}/$${LIB} $${LIB_DST};				\
			elif test -f $${LIB_DIR}/$${LIB}; then					\
				cp $${LIB_DIR}/$${LIB} $${LIB_DST};				\
				case "$${STRIP}" in						\
				(0 | n | no)							\
					;;							\
				(*)								\
					$(TARGET_CROSS)strip "$${LIB_DST}/$${LIB_BASENAME}";	\
					;;							\
				esac;								\
			else									\
				exit -1;							\
			fi;									\
			ln -s $${LIB} $(TARGET_DIR)$${DST}/$${LIB_BASENAME};			\
			LIB="`readlink $${LIB_DIR}/$${LIB}`";					\
		done;										\
	done;											\
												\
	echo -n

uclibc: dependencies $(TARGET_DIR)/lib/$(strip $(subst ",, $(BR2_TOOLCHAIN_EXTERNAL_LIB_C)))

$(TARGET_DIR)/lib/$(strip $(subst ",, $(BR2_TOOLCHAIN_EXTERNAL_LIB_C))):
#"))
	mkdir -p $(TARGET_DIR)/lib
	@$(call copy_toolchain_lib_root, $(strip $(subst ",, $(BR2_TOOLCHAIN_EXTERNAL_LIB_C))), /lib, $(BR2_TOOLCHAIN_EXTERNAL_STRIP))
#")))
	for libs in $(strip $(subst ",, $(BR2_TOOLCHAIN_EXTERNAL_LIBS))) ; do \
		$(call copy_toolchain_lib_root, $$libs, /lib, $(BR2_TOOLCHAIN_EXTERNAL_STRIP)) ; \
	done
