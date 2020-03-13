ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = \
	LICENSE CHANGELOG.md INSTALL.md README.md \
	.gitignore

MAINTAINERCLEANFILES = 
DISTCLEANFILES =
CLEANFILES = 
BUILT_SOURCES = 

noinst_PROGRAMS =
bin_PROGRAMS = \
	eval gentransform tlookup

CPPFLAGS = $(LIBJANSSON_CFLAGS)
LDADD = $(LIBJANSSON_LIBS)

eval_LDADD = 
eval_SOURCES = eval.cc

gentransform_LDADD = $(LDADD)
gentransform_SOURCES = gentransform.cc context.h database.h

tlookup_LDADD = $(LDADD)
tlookup_SOURCES = tlookup.cc context.h database.h
