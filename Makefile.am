ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = \
	LICENSE CHANGELOG.md INSTALL.md README.md \
	.gitignore

MAINTAINERCLEANFILES = restartdata.h
DISTCLEANFILES =
MOSTLYCLEANFILES =
CLEANFILES =
BUILT_SOURCES = genrestartdata

noinst_PROGRAMS =
bin_PROGRAMS = \
	eval gentransform tlookup gensignature genrestartdata genmember slookup genhint selftest genswap

# genrestartdata is slow and output is small, include in distribution
restartdata.h : genrestartdata.cc
	@echo "#"
	@echo "# If you are not developing when this happens, most likely timestamps are incorrect"
	@echo "# Instead of waiting 60 minutes:"
	@echo "# - Restore pre-calculated version of \"restartdata.h\"'"
	@echo "# - Perform \"touch restartdata.h\""
	@echo "#"
	./genrestartdata > restartdata.h

AM_CPPFLAGS = $(LIBJANSSON_CFLAGS)
AM_LDADD = $(LIBJANSSON_LIBS)
AM_CXXFLAGS =  -Wall -funroll-loops -finline -msse4

# @date 2020-03-06 16:56:25
eval_SOURCES = eval.cc

# @date 2020-03-11 21:53:16
gentransform_SOURCES = gentransform.cc database.h datadef.h context.h tinytree.h
gentransform_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-03-13 12:56:11
tlookup_SOURCES = tlookup.cc database.h datadef.h context.h tinytree.h
tlookup_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-03-14 11:09:15
gensignature_SOURCES = gensignature.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
gensignature_LDADD = $(LDADD) $(AM_LDADD)
gensignature.$(OBJEXT) : restartdata.h

# @date 2020-03-18 18:04:50
genrestartdata_SOURCES = genrestartdata.cc tinytree.h context.h datadef.h generator.h metrics.h

# @date 2020-03-30 17:19:24
genmember_SOURCES = genmember.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
genmember_LDADD = $(LDADD) $(AM_LDADD)
genmember.$(OBJEXT) : restartdata.h

# @date 2020-04-07 16:25:18
slookup_SOURCES = slookup.cc tinytree.h context.h datadef.h database.h
slookup_LDADD = $(LDADD) $(AM_LDADD)
slookup.$(OBJEXT) : restartdata.h

# @date 2020-04-18 20:46:40
genhint_SOURCES = genhint.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h
genhint_LDADD = $(LDADD) $(AM_LDADD)
genhint.$(OBJEXT) : restartdata.h

# @date 2020-04-21 23:30:30
selftest_SOURCES = selftest.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
selftest_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-05-02 23:02:57
genswap_SOURCES = genswap.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h
genswap_LDADD = $(LDADD) $(AM_LDADD)
genswap.$(OBJEXT) : restartdata.h
