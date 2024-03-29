ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = \
	LICENSE.txt BUILD.md CHANGELOG.md GLOSSARY.md README.md \
	.gitignore $(EXTRA_PART1) $(EXTRA_PART2) $(EXTRA_PART3) $(EXTRA_PART4) $(EXTRA_PART5) $(EXTRA_PART6)

MAINTAINERCLEANFILES = restartdata.h
DISTCLEANFILES =
MOSTLYCLEANFILES =
CLEANFILES =
BUILT_SOURCES = genrestartdata rewritedata.h

noinst_PROGRAMS =
bin_PROGRAMS = $(PROGRAMS_PART1) $(PROGRAMS_PART2) $(PROGRAMS_PART3) $(PROGRAMS_PART4) $(PROGRAMS_PART5) $(PROGRAMS_PART6)

##
## This section for dataset creation utilities
##

PROGRAMS_PART1 = evaluate genmember genrestartdata gensignature genswap gentransform selftest slookup tlookup
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
evaluate_SOURCES = evaluate.cc

# @date 2020-03-30 17:19:24
genmember_SOURCES = genmember.cc genmember.h database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
genmember_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-03-18 18:04:50
genrestartdata_SOURCES = genrestartdata.cc tinytree.h context.h datadef.h generator.h metrics.h

# @date 2020-03-14 11:09:15
gensignature_SOURCES = gensignature.cc gensignature.h database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
gensignature_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-05-02 23:02:57
genswap_SOURCES = genswap.cc genswap.h database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h
genswap_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-03-11 21:53:16
gentransform_SOURCES = gentransform.cc gentransform.h database.h datadef.h context.h tinytree.h
gentransform_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-04-21 23:30:30
selftest_SOURCES = selftest.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h  grouptree.h
selftest_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-04-07 16:25:18
slookup_SOURCES = slookup.cc tinytree.h context.h datadef.h database.h
slookup_LDADD = $(LDADD) $(AM_LDADD)

# @date 2020-03-13 12:56:11
tlookup_SOURCES = tlookup.cc database.h datadef.h context.h tinytree.h
tlookup_LDADD = $(LDADD) $(AM_LDADD)

##
## This section for creation of examples
##

PROGRAMS_PART2 = build7bitCount build9bitAdder build9bitTable buildaes builddes buildmd5 buildspongent buildtest0
EXTRA_PART2 = genvalidateaes.js genvalidatedes.js genvalidatemd5.js genvalidatespongent.js

# @date 2021-07-21 11:04:05
build7bitCount_SOURCES = build7bitCount.cc basetree.h context.h
build7bitCount_LDADD = $(LDADD) $(AM_LDADD)

# 2021-07-22 22:37:56
build9bitAdder_SOURCES = build9bitAdder.cc basetree.h context.h
build9bitAdder_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-15 18:53:07
build9bitTable_SOURCES = build9bitTable.cc basetree.h context.h
build9bitTable_LDADD = $(LDADD) $(AM_LDADD)

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

##
## This section for creation of tools for `baseTree_t`
##

PROGRAMS_PART3 = bjoin bload bsave bvalidate kslice validateprefix
EXTRA_PART3 =

# @date 2021-05-20 22:51:00
bjoin_SOURCES = bjoin.cc basetree.h context.h
bjoin_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-26 23:42:25
bload_SOURCES = bload.cc basetree.h context.h
bload_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-21 00:41:38
bsave_SOURCES = bsave.cc basetree.h context.h
bsave_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-13 15:47:59
bvalidate_SOURCES = bvalidate.cc basetree.h context.h
bvalidate_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-19 14:20:16
kslice_SOURCES = kslice.cc basetree.h context.h
kslice_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-05-22 18:54:24
validateprefix_SOURCES = validateprefix.cc basetree.h context.h
validateprefix_LDADD = $(LDADD) $(AM_LDADD)

##
## This section for extraction of information
##

PROGRAMS_PART4 = bextract bfold bsystem 
EXTRA_PART4 =

# @date 2021-06-05 21:35:41
bextract_SOURCES = bextract.cc basetree.h context.h
bextract_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-05 23:43:44
bfold_SOURCES = bfold.cc basetree.h context.h rewritetree.h
bfold_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-05 13:58:33
bsystem_SOURCES = bsystem.cc basetree.h context.h
bsystem_LDADD = $(LDADD) $(AM_LDADD)

##
## This section for baseTree optimisations
##

PROGRAMS_PART5 = beval gendepreciate genexport genimport genrewritedata validaterewrite
EXTRA_PART5 =

rewritedata.c : genrewritedata.cc
	./genrewritedata > rewritedata.c

# @date 2021-06-08 10:19:45
beval_SOURCES = beval.cc basetree.h context.h rewritetree.h
beval_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-27 15:50:25
gendepreciate_SOURCES = gendepreciate.cc gendepreciate.h database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h
gendepreciate_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-07-15 23:49:33
genexport_SOURCES = genexport.cc genport.h basetree.h context.h
genexport_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-07-16 12:43:42
genimport_SOURCES = genimport.cc genport.h gentransform.h gensignature.h genswap.h genmember.h basetree.h context.h
genimport_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-10 11:39:02
genrewritedata_SOURCES = genrewritedata.cc database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h restartdata.h
genrewritedata_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-06-10 11:39:02
validaterewrite_SOURCES = validaterewrite.cc rewritedata.h basetree.h context.h rewritedata.c
validaterewrite_LDADD = $(LDADD) $(AM_LDADD)

##
## This section for groupTree front-ends
##

PROGRAMS_PART6 = genpattern geval gfold gload glookup gsave gsystem gvalidate
EXTRA_PART6 =

# @date 2021-11-02 22:32:07
geval_SOURCES = geval.cc grouptree.h context.h
geval_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-10-17 17:51:31
genpattern_SOURCES = genpattern.cc genpattern.h database.h datadef.h context.h tinytree.h dbtool.h generator.h metrics.h
genpattern_LDADD = $(LDADD) $(AM_LDADD)

# @date 2022-01-28 15:37:52
gfold_SOURCES = gfold.cc grouptree.h context.h database.h datadef.h dbtool.h 
gfold_LDADD = $(LDADD) $(AM_LDADD)

# @date 2022-01-28 19:47:44
gload_SOURCES = gload.cc grouptree.h context.h database.h datadef.h dbtool.h 
gload_LDADD = $(LDADD) $(AM_LDADD)

# @date 2021-11-26 23:18:07
glookup_SOURCES = glookup.cc database.h datadef.h dbtool.h context.h tinytree.h basetree.h grouptree.h
glookup_LDADD = $(LDADD) $(AM_LDADD)

# @date 2022-02-16 23:15:33
gsave_SOURCES = gsave.cc grouptree.h context.h database.h datadef.h dbtool.h 
gsave_LDADD = $(LDADD) $(AM_LDADD)

# @date 2022-02-16 22:47:04
gsystem_SOURCES = gsystem.cc grouptree.h context.h database.h datadef.h dbtool.h 
gsystem_LDADD = $(LDADD) $(AM_LDADD)

# @date 2022-02-13 15:31:19
gvalidate_SOURCES = gvalidate.cc grouptree.h context.h database.h datadef.h dbtool.h 
gvalidate_LDADD = $(LDADD) $(AM_LDADD)
