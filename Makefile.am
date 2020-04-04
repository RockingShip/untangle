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
	eval gentransform tlookup gensignature genrestartdata genmember

# genrestartdata is slow and output is small, include in distribution
restartdata.h : genrestartdata.cc
	@echo "#"
	@echo "# If you are not developing when this happens, most likely timestamps are incorrect"
	@echo "# Instead of waiting 6 minutes:"
	@echo "# - Restore pre-calculated version of \"restartdata.h\"'"
	@echo "# - Perform \"touch restartdata.h\""
	@echo "#"
	./genrestartdata > restartdata.h

CPPFLAGS = $(LIBJANSSON_CFLAGS)
LDADD = $(LIBJANSSON_LIBS)

# @date 2020-03-06 16:56:25
eval_LDADD = 
eval_SOURCES = eval.cc

# @date 2020-03-11 21:53:16
gentransform_LDADD = $(LDADD)
gentransform_SOURCES = gentransform.cc database.h datadef.h context.h tinytree.h

# @date 2020-03-13 12:56:11
tlookup_LDADD = $(LDADD)
tlookup_SOURCES = tlookup.cc database.h datadef.h context.h tinytree.h

# @date 2020-03-14 11:09:15
gensignature_LDADD = $(LDADD)
gensignature_SOURCES = gensignature.cc tinytree.h context.h datadef.h database.h generator.h restartdata.h metrics.h
gensignature.$(OBJEXT) : restartdata.h

# @date 2020-03-18 18:04:50
genrestartdata_SOURCES = genrestartdata.cc tinytree.h context.h datadef.h generator.h metrics.h

# @date 2020-03-30 17:19:24
genmember_LDADD = $(LDADD)
genmember_SOURCES = genmember.cc tinytree.h context.h datadef.h database.h generator.h restartdata.h metrics.h
genmember.$(OBJEXT) : restartdata.h
