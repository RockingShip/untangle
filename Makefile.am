ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = \
	LICENSE CHANGELOG.md INSTALL.md README.md \
	.gitignore

MAINTAINERCLEANFILES = 
DISTCLEANFILES =
CLEANFILES = pushdata.h
BUILT_SOURCES = genprimedata genpushdata pushdata.h genrestartdata

noinst_PROGRAMS =
bin_PROGRAMS = \
	eval gentransform tlookup gensignature genprimedata genpushdata genrestartdata

# genprimedata is fast and output is small, include in distribution
primedata.h : genprimedata.cc
	./genprimedata > primedata.h

# pushdata is fast and output is big, exclude from distribution
pushdata.h : genpushdata.cc
	./genpushdata > pushdata.h

# genrestartdata is slow and output is small, include in distribution
restartdata.h : genrestartdata.cc
	@echo "#"
	@echo "# If you are not developing when this happens, most likely timestamps are incorrect"
	@echo "# Instead of waiting 6 minutes:
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
gentransform_SOURCES = gentransform.cc database.h datadef.h context.h primedata.h tinytree.h

# @date 2020-03-13 12:56:11
tlookup_LDADD = $(LDADD)
tlookup_SOURCES = tlookup.cc database.h datadef.h context.h primedata.h tinytree.h

# @date 2020-03-14 11:09:15
gensignature_LDADD = $(LDADD)
gensignature_SOURCES = gensignature.cc tinytree.h context.h primedata.h datadef.h database.h primedata.h metrics.h

# @date 2020-03-14 18:11:10
genprimedata_SOURCES = genprimedata.cc

# @date 2020-03-18 10:50:29
genpushdata_SOURCES = genpushdata.cc

# @date 2020-03-18 18:04:50
# NOTE: do not include restartdata.h with dependencies
genrestartdata_SOURCES = genrestartdata.cc generator.h tinytree.h context.h primedata.h datadef.h restartdata.h metrics.h
nodist_genrestartdata_SOURCES = pushdata.h
