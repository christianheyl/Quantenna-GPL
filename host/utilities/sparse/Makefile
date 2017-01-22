
all: install

install: src
	(cd src && make && make install PREFIX=../install)

src:
	mkdir src
	(cd src && git clone git://git.kernel.org/pub/scm/devel/sparse/sparse.git .)

