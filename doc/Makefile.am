RM=rm
MANDIR=$(DESTDIR)@mandir@/man1

XSLTPROC=xsltproc
XSLTPROC_FLAGS=@XSLTPROC_FLAGS@
XHTML_STYLESHEET=$(srcdir)/xsl/xhtml.xsl
CHUNK_XHTML_STYLESHEET=$(srcdir)/xsl/xhtml-chunk.xsl
XML_CATALOG_FILES=xsl/catalog.xml
STYLESHEETS=$(CHUNK_XHTML_STYLESHEET) $(srcdir)/xsl/xhtml-common.xsl

generated_mans = oprofiled.1 op_help.1 oprofpp.1 op_time.1 op_to_source.1 op_merge.1 opcontrol.1

man_MANS = oprofile.1 $(generated_mans)

htmldir = $(prefix)/share/doc/oprofile
dist_html_DATA = oprofile.html

$(generated_mans):
	echo ".so man1/oprofile.1" > $@

if have_xsltproc

oprofile.html: ${top_srcdir}/doc/oprofile.xml
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ --stringparam version @VERSION@ $(XHTML_STYLESHEET) $<

# rules to generate oprofile.sf.net/doc files
chunk: ${top_srcdir}/doc/oprofile.xml
	$(XSLTPROC) $(XSLTPROC_FLAGS) --stringparam version @VERSION@ $(CHUNK_XHTML_STYLESHEET) $<

else

oprofile.html:
	touch $@

chunk:

endif

distclean-local:
	$(RM) -f xsl/catalog-1.xml xsl/catalog.xml

clean-local:
	$(RM) -f $(generated_mans)

# these files are not cleaned by make uninstall automake bug ?
uninstall-local:
	rm -f @mandir@/cat1/oprofile.1.gz
	@for f in $(LINK_LIST); do		\
		rm -f $(CATDIR)/cat1/$$f.gz;	\
	done

EXTRA_DIST = oprofile.1 oprofile.1.in oprofile.xml xsl/catalog-1.xml.in xsl/xhtml.xsl xsl/xhtml-common.xsl xsl/xhtml-chunk.xsl srcdoc/Doxyfile srcdoc/Makefile

