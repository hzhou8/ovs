# OVN southbound schema and IDL
EXTRA_DIST += ovn/ovn-sb.ovsschema
pkgdata_DATA += ovn/ovn-sb.ovsschema

# OVN southbound E-R diagram
#
# If "python" or "dot" is not available, then we do not add graphical diagram
# to the documentation.
if HAVE_PYTHON
if HAVE_DOT
ovn/ovn-sb.gv: ovsdb/ovsdb-dot.in ovn/ovn-sb.ovsschema
	$(AM_V_GEN)$(OVSDB_DOT) --no-arrows $(srcdir)/ovn/ovn-sb.ovsschema > $@
ovn/ovn-sb.pic: ovn/ovn-sb.gv ovsdb/dot2pic
	$(AM_V_GEN)(dot -T plain < ovn/ovn-sb.gv | $(PYTHON) $(srcdir)/ovsdb/dot2pic -f 3) > $@.tmp && \
	mv $@.tmp $@
OVN_SB_PIC = ovn/ovn-sb.pic
OVN_SB_DOT_DIAGRAM_ARG = --er-diagram=$(OVN_SB_PIC)
CLEANFILES += ovn/ovn-sb.gv ovn/ovn-sb.pic
endif
endif

# OVN southbound schema documentation
EXTRA_DIST += ovn/ovn-sb.xml
CLEANFILES += ovn/ovn-sb.5
man_MANS += ovn/ovn-sb.5
ovn/ovn-sb.5: \
	ovsdb/ovsdb-doc ovn/ovn-sb.xml ovn/ovn-sb.ovsschema $(OVN_SB_PIC)
	$(AM_V_GEN)$(OVSDB_DOC) \
		$(OVN_SB_DOT_DIAGRAM_ARG) \
		--version=$(VERSION) \
		$(srcdir)/ovn/ovn-sb.ovsschema \
		$(srcdir)/ovn/ovn-sb.xml > $@.tmp && \
	mv $@.tmp $@

# OVN northbound schema and IDL
EXTRA_DIST += ovn/ovn-nb.ovsschema
pkgdata_DATA += ovn/ovn-nb.ovsschema

# OVN northbound E-R diagram
#
# If "python" or "dot" is not available, then we do not add graphical diagram
# to the documentation.
if HAVE_PYTHON
if HAVE_DOT
ovn/ovn-nb.gv: ovsdb/ovsdb-dot.in ovn/ovn-nb.ovsschema
	$(AM_V_GEN)$(OVSDB_DOT) --no-arrows $(srcdir)/ovn/ovn-nb.ovsschema > $@
ovn/ovn-nb.pic: ovn/ovn-nb.gv ovsdb/dot2pic
	$(AM_V_GEN)(dot -T plain < ovn/ovn-nb.gv | $(PYTHON) $(srcdir)/ovsdb/dot2pic -f 3) > $@.tmp && \
	mv $@.tmp $@
OVN_NB_PIC = ovn/ovn-nb.pic
OVN_NB_DOT_DIAGRAM_ARG = --er-diagram=$(OVN_NB_PIC)
CLEANFILES += ovn/ovn-nb.gv ovn/ovn-nb.pic
endif
endif

# OVN northbound schema documentation
EXTRA_DIST += ovn/ovn-nb.xml
CLEANFILES += ovn/ovn-nb.5
man_MANS += ovn/ovn-nb.5
ovn/ovn-nb.5: \
	ovsdb/ovsdb-doc ovn/ovn-nb.xml ovn/ovn-nb.ovsschema $(OVN_NB_PIC)
	$(AM_V_GEN)$(OVSDB_DOC) \
		$(OVN_NB_DOT_DIAGRAM_ARG) \
		--version=$(VERSION) \
		$(srcdir)/ovn/ovn-nb.ovsschema \
		$(srcdir)/ovn/ovn-nb.xml > $@.tmp && \
	mv $@.tmp $@

# OVN interconnection southbound schema and IDL
EXTRA_DIST += ovn/ovn-isb.ovsschema
pkgdata_DATA += ovn/ovn-isb.ovsschema

# OVN interconnection southbound E-R diagram
#
# If "python" or "dot" is not available, then we do not add graphical diagram
# to the documentation.
if HAVE_PYTHON
if HAVE_DOT
ovn/ovn-isb.gv: ovsdb/ovsdb-dot.in ovn/ovn-isb.ovsschema
	$(AM_V_GEN)$(OVSDB_DOT) --no-arrows $(srcdir)/ovn/ovn-isb.ovsschema > $@
ovn/ovn-isb.pic: ovn/ovn-isb.gv ovsdb/dot2pic
	$(AM_V_GEN)(dot -T plain < ovn/ovn-isb.gv | $(PYTHON) $(srcdir)/ovsdb/dot2pic -f 3) > $@.tmp && \
	mv $@.tmp $@
OVN_ISB_PIC = ovn/ovn-isb.pic
OVN_ISB_DOT_DIAGRAM_ARG = --er-diagram=$(OVN_ISB_PIC)
CLEANFILES += ovn/ovn-isb.gv ovn/ovn-isb.pic
endif
endif

# OVN interconnection southbound schema documentation
EXTRA_DIST += ovn/ovn-isb.xml
CLEANFILES += ovn/ovn-isb.5
man_MANS += ovn/ovn-isb.5
ovn/ovn-isb.5: \
	ovsdb/ovsdb-doc ovn/ovn-isb.xml ovn/ovn-isb.ovsschema $(OVN_ISB_PIC)
	$(AM_V_GEN)$(OVSDB_DOC) \
		$(OVN_ISB_DOT_DIAGRAM_ARG) \
		--version=$(VERSION) \
		$(srcdir)/ovn/ovn-isb.ovsschema \
		$(srcdir)/ovn/ovn-isb.xml > $@.tmp && \
	mv $@.tmp $@

# OVN interconnection northbound schema and IDL
EXTRA_DIST += ovn/ovn-inb.ovsschema
pkgdata_DATA += ovn/ovn-inb.ovsschema

# OVN interconnection northbound E-R diagram
#
# If "python" or "dot" is not available, then we do not add graphical diagram
# to the documentation.
if HAVE_PYTHON
if HAVE_DOT
ovn/ovn-inb.gv: ovsdb/ovsdb-dot.in ovn/ovn-inb.ovsschema
	$(AM_V_GEN)$(OVSDB_DOT) --no-arrows $(srcdir)/ovn/ovn-inb.ovsschema > $@
ovn/ovn-inb.pic: ovn/ovn-inb.gv ovsdb/dot2pic
	$(AM_V_GEN)(dot -T plain < ovn/ovn-inb.gv | $(PYTHON) $(srcdir)/ovsdb/dot2pic -f 3) > $@.tmp && \
	mv $@.tmp $@
OVN_INB_PIC = ovn/ovn-inb.pic
OVN_INB_DOT_DIAGRAM_ARG = --er-diagram=$(OVN_INB_PIC)
CLEANFILES += ovn/ovn-inb.gv ovn/ovn-inb.pic
endif
endif

# OVN interconnection northbound schema documentation
EXTRA_DIST += ovn/ovn-inb.xml
CLEANFILES += ovn/ovn-inb.5
man_MANS += ovn/ovn-inb.5
ovn/ovn-inb.5: \
	ovsdb/ovsdb-doc ovn/ovn-inb.xml ovn/ovn-inb.ovsschema $(OVN_INB_PIC)
	$(AM_V_GEN)$(OVSDB_DOC) \
		$(OVN_INB_DOT_DIAGRAM_ARG) \
		--version=$(VERSION) \
		$(srcdir)/ovn/ovn-inb.ovsschema \
		$(srcdir)/ovn/ovn-inb.xml > $@.tmp && \
	mv $@.tmp $@

man_MANS += ovn/ovn-architecture.7
EXTRA_DIST += ovn/ovn-architecture.7.xml
CLEANFILES += ovn/ovn-architecture.7

EXTRA_DIST += \
	ovn/TODO.rst

# Version checking for ovn-nb.ovsschema.
ALL_LOCAL += ovn/ovn-nb.ovsschema.stamp
ovn/ovn-nb.ovsschema.stamp: ovn/ovn-nb.ovsschema
	$(srcdir)/build-aux/cksum-schema-check $? $@
CLEANFILES += ovn/ovn-nb.ovsschema.stamp

# Version checking for ovn-sb.ovsschema.
ALL_LOCAL += ovn/ovn-sb.ovsschema.stamp
ovn/ovn-sb.ovsschema.stamp: ovn/ovn-sb.ovsschema
	$(srcdir)/build-aux/cksum-schema-check $? $@
CLEANFILES += ovn/ovn-sb.ovsschema.stamp

# Version checking for ovn-inb.ovsschema.
ALL_LOCAL += ovn/ovn-inb.ovsschema.stamp
ovn/ovn-inb.ovsschema.stamp: ovn/ovn-inb.ovsschema
	$(srcdir)/build-aux/cksum-schema-check $? $@
CLEANFILES += ovn/ovn-inb.ovsschema.stamp

# Version checking for ovn-isb.ovsschema.
ALL_LOCAL += ovn/ovn-isb.ovsschema.stamp
ovn/ovn-isb.ovsschema.stamp: ovn/ovn-isb.ovsschema
	$(srcdir)/build-aux/cksum-schema-check $? $@
CLEANFILES += ovn/ovn-isb.ovsschema.stamp

include ovn/controller/automake.mk
include ovn/controller-vtep/automake.mk
include ovn/lib/automake.mk
include ovn/northd/automake.mk
include ovn/utilities/automake.mk
