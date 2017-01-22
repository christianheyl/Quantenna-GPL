DOXYGEN_PDF_INPUT_FILES = ../../../buildroot/package/qcsapi/qcsapi-1.0.1/qcsapi.h \
	groupdefine.txt chapter1_2_3.txt chapter4.txt chapter5.txt chapter7.txt chapter8.txt \
	../../../drivers/include/shared/qtn/qvsp_data.h \
	../../../include/qtn/shared_defs.h

DOXYGEN_PDF_INPUT_FILE_INTERNAL_ONLY = vsp.txt

DOXYGEN_PDF_INPUT_FILES += $(shell ls -r changelog/*changes-* | sort -Vr)

TARGET_DIR = ../../../doxygen/qcsapi
TARGETS = $(TARGET_DIR)/Quantenna_QCSAPI_internal.pdf $(TARGET_DIR)/Quantenna_QCSAPI.pdf

all: doxygen_pdf
doxygen_pdf: $(TARGETS)

-include vsp_doc.d

vsp_doc: vsp_doc.c
	gcc -Wall -Werror -ansi -pedantic -O1 -g				\
		-isystem ../../../drivers/include/shared 			\
		-isystem ../../../include					\
		-isystem ../../../						\
		-isystem ../../../buildroot/package/qcsapi/qcsapi-1.0.1/	\
		$< -o $@ -MD -MF $@.d

vsp.txt: vsp_doc
	./$< > $@

Doxyfile_pdf.Quantenna_QCSAPI: $(DOXYGEN_PDF_INPUT_FILES)
	perl ../qtn_doxyfile_pdf.pl	\
		Doxyfile_pdf		\
		"Quantenna Configuration and Status APIs"	\
		"qcsapi"		\
		"$(REV_NUM)"		\
		--predefined "DOXYGEN_EXCLUDE" $^ > $@

Doxyfile_pdf.Quantenna_QCSAPI_internal: $(DOXYGEN_PDF_INPUT_FILES) $(DOXYGEN_PDF_INPUT_FILE_INTERNAL_ONLY)
	perl ../qtn_doxyfile_pdf.pl	\
		Doxyfile_pdf		\
		"Quantenna Configuration and Status APIs INTERNAL ONLY"	\
		"qcsapi" "$(REV_NUM)"	\
		--internal $^ > $@

$(TARGET_DIR)/%.pdf: Doxyfile_pdf.%
	rm -rf $(TARGET_DIR)/latex
	doxygen $<
	./qtn_latex.sh
	make -C $(TARGET_DIR)/latex
	pdftk ../qcsapi-title.pdf stamp ../../rev-num.pdf output title.pdf
	pdftk A=$(TARGET_DIR)/latex/refman.pdf B=./title.pdf cat B1 A2-end output xxx.pdf
	mv xxx.pdf $@
	-rm title.pdf $(TARGET_DIR)/latex/refman.pdf

clean:
	rm -f Doxyfile_pdf*
	rm -rf $(TARGET_DIR)
