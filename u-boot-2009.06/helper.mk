
RUBY_MINI_LDMAP_SRC = board/ruby_mini/u-boot.lds.S

define build-mini-ldmap
	@mkdir -p ${@D}
	$(CPP) -D__ASSEMBLY__ -I../common -Iinclude $1 $< | grep -vE '^\#' > $@
endef

