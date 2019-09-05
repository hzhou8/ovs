# ovn-ic
bin_PROGRAMS += ovn/ic/ovn-ic
ovn_ic_ovn_ic_SOURCES = ovn/ic/ovn-ic.c
ovn_ic_ovn_ic_LDADD = \
	ovn/lib/libovn.la \
	ovsdb/libovsdb.la \
	lib/libopenvswitch.la
man_MANS += ovn/ic/ovn-ic.8
EXTRA_DIST += ovn/ic/ovn-ic.8.xml
CLEANFILES += ovn/ic/ovn-ic.8
