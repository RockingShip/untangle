ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = \
	LICENSE CHANGELOG.md INSTALL.md README.md \
	.gitignore

MAINTAINERCLEANFILES = 
DISTCLEANFILES =
CLEANFILES = 
BUILT_SOURCES = genprimedata

noinst_PROGRAMS =
bin_PROGRAMS = \
	eval gentransform tlookup gensignature genprimedata

primedata.cc : genprimedata.cc
	./genprimedata > primedata.cc

CPPFLAGS = $(LIBJANSSON_CFLAGS)
LDADD = $(LIBJANSSON_LIBS)

# @date 2020-03-06 16:56:25
eval_LDADD = 
eval_SOURCES = eval.cc

# @date 2020-03-11 21:53:16
gentransform_LDADD = $(LDADD)
gentransform_SOURCES = gentransform.cc context.h database.h

# @date 2020-03-13 12:56:11
tlookup_LDADD = $(LDADD)
tlookup_SOURCES = tlookup.cc context.h database.h

# @date 2020-03-14 11:09:15
gensignature_LDADD = $(LDADD)
gensignature_SOURCES = gensignature.cc context.h database.h tinytree.h primedata.cc

# @date 2020-03-14 18:11:10
genprimedata_SOURCES = genprimedata.cc
