ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = \
	LICENSE CHANGELOG.md INSTALL.md README.md \
	.gitignore

MAINTAINERCLEANFILES = 
DISTCLEANFILES =
CLEANFILES = 
BUILT_SOURCES = genprimedata genpushdata

noinst_PROGRAMS =
bin_PROGRAMS = \
	eval gentransform tlookup gensignature genprimedata genpushdata

primedata.h : genprimedata.cc
	./genprimedata > primedata.h

pushdata.h : genpushdata.cc
	./genpushdata > pushdata.h

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