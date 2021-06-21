ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = \
	LICENSE.txt BUILD.md CHANGELOG.md GLOSSARY.md README.md \
	.gitignore $(EXTRA_PART1) $(EXTRA_PART2) $(EXTRA_PART3) $(EXTRA_PART4)

MAINTAINERCLEANFILES = restartdata.h
DISTCLEANFILES =
MOSTLYCLEANFILES =
CLEANFILES =
BUILT_SOURCES = genrestartdata

noinst_PROGRAMS =
bin_PROGRAMS = $(PROGRAMS_PART1) $(PROGRAMS_PART2) $(PROGRAMS_PART3) $(PROGRAMS_PART4)

##
## This section for dataset creation utilities
##

PROGRAMS_PART1 = eval genhint genmember genrestartdata gensignature genswap gentransform selftest slookup tlookup
EXTRA_PART1 =

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
AM_CXXFLAGS =  -Wall -Werror -funroll-loops -finline -msse4

# @date 2020-03-06 16:56:25
eval_SOURCES = eval.cc

# @date 2020-04-18 20:46:40
genhint_SOURCES = genhint.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h
genhint_LDADD = $(LDADD) $(AM_LDADD)
genhint.$(OBJEXT) : restartdata.h

# @date 2020-03-30 17:19:24
genmember_SOURCES = genmember.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
genmember_LDADD = $(LDADD) $(AM_LDADD)
genmember.$(OBJEXT) : restartdata.h

# @date 2020-03-18 18:04:50
genrestartdata_SOURCES = genrestartdata.cc tinytree.h context.h datadef.h generator.h metrics.h

# @date 2020-03-14 11:09:15
gensignature_SOURCES = gensignature.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
gensignature_LDADD = $(LDADD) $(AM_LDADD)
gensignature.$(OBJEXT) : restartdata.h

# @date 2020-05-02 23:02:57
genswap_SOURCES = genswap.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h
genswap_LDADD = $(LDADD) $(AM_LDADD)
genswap.$(OBJEXT) : restartdata.h

# @date 2020-03-11 21:53:16
gentransform_SOURCES = gentransform.cc database.h datadef.h context.h tinytree.h
gentransform_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-04-21 23:30:30
selftest_SOURCES = selftest.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
selftest_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-04-07 16:25:18
slookup_SOURCES = slookup.cc tinytree.h context.h datadef.h database.h
slookup_LDADD = $(LDADD) $(AM_LDADD)
slookup.$(OBJEXT) : restartdata.h

# @date 2020-03-13 12:56:11
tlookup_SOURCES = tlookup.cc database.h datadef.h context.h tinytree.h
tlookup_LDADD = $(LDADD) $(AM_LDADD)

##
## This section for creation of examples
##

PROGRAMS_PART2 = build9bit buildaes builddes buildmd5 buildspongent buildtest0 kjoin kload ksave kslice spongent validate validateprefix
EXTRA_PART2 = genvalidateaes.js genvalidatedes.js genvalidatemd5.js genvalidatespongent.js

# @date 2021-05-15 18:53:07
build9bit_SOURCES = build9bit.cc basetree.h context.h
build9bit_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-17 16:49:11
buildaes_SOURCES = buildaes.cc buildaes.h buildaesbox.h validateaes.h
buildaes_CXXFLAGS = -fno-var-tracking-assignments # was expecting this disabled with `__attribute__((optimize("O0")))`
buildaes_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-09 23:51:48
builddes_SOURCES = builddes.cc builddes.h builddesbox.h validatedes.h
builddes_CXXFLAGS = -fno-var-tracking-assignments
builddes_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-16 22:47:55
buildmd5_SOURCES = buildmd5.cc buildmd5.h validatemd5.h
buildmd5_CXXFLAGS = -fno-var-tracking-assignments
buildmd5_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-17 14:36:48
buildspongent_SOURCES = buildspongent.cc buildspongent.h buildspongentbox.h validatespongent.h
buildspongent_CXXFLAGS = -fno-var-tracking-assignments
buildspongent_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-15 12:41:29
buildtest0_SOURCES = buildtest0.cc basetree.h context.h
buildtest0_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-20 22:51:00
kjoin_SOURCES = kjoin.cc basetree.h context.h
kjoin_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-26 23:42:25
kload_SOURCES = kload.cc basetree.h context.h
kload_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-21 00:41:38
ksave_SOURCES = ksave.cc basetree.h context.h
ksave_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-19 14:20:16
kslice_SOURCES = kslice.cc basetree.h context.h
kslice_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-17 14:21:07
spongent_SOURCES = spongent.cc spongent.h
spongent_CXXFLAGS = -D_SPONGENT088080008_

# @date 2021-05-13 15:47:59
validate_SOURCES = validate.cc basetree.h context.h
validate_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-22 18:54:24
validateprefix_SOURCES = validateprefix.cc basetree.h context.h
validateprefix_LDADD = $(LDADD) $(AM_LDADD)

##
## This section for extraction of information
##

PROGRAMS_PART3 = kextract kfold ksystem
EXTRA_PART3 =

# @date 2021-06-05 21:35:41
kextract_SOURCES = kextract.cc basetree.h context.h
kextract_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-05 23:43:44
kfold_SOURCES = kfold.cc basetree.h context.h
kfold_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-05 13:58:33
ksystem_SOURCES = ksystem.cc basetree.h context.h
ksystem_LDADD = $(LDADD) $(AM_LDADD)

##
## This section for baseTree optimisations
##

PROGRAMS_PART4 = beval genprime validateprime validaterewrite
EXTRA_PART4 =

# @date 2021-06-08 10:19:45
beval_SOURCES = beval.cc basetree.h context.h
beval_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-17 17:48:38
genprime_SOURCES = genprime.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
genprime_LDADD = $(LDADD) $(AM_LDADD)
genprime.$(OBJEXT) : restartdata.h

# @date 2021-06-17 23:50:23
validateprime_SOURCES = validateprime.cc basetree.h context.h
validateprime_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-10 11:39:02
validaterewrite_SOURCES = validaterewrite.cc rewritedata.h basetree.h context.h rewritedata.c
validaterewrite_LDADD = $(LDADD) $(AM_LDADD)
validaterewrite.$(OBJEXT) : restartdata.h
