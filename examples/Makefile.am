noinst_PROGRAMS=	diploid_ind \
	diploid_fixed_sh_ind \
	migsel_ind \
	bneck_selection_ind \
	diploid_ind_2locus \
	K_linked_regions_multilocus \
	K_linked_regions_extensions \
	K_linked_regions_generalized_rec \
	HOC_ind 


diploid_ind_SOURCES=diploid_ind.cc common_ind.hpp
diploid_fixed_sh_ind_SOURCES=diploid_fixed_sh_ind.cc common_ind.hpp
migsel_ind_SOURCES=migsel_ind.cc common_ind.hpp
bneck_selection_ind_SOURCES=bneck_selection_ind.cc common_ind.hpp
diploid_ind_2locus_SOURCES=diploid_ind_2locus.cc common_ind.hpp
K_linked_regions_multilocus_SOURCES=K_linked_regions_multilocus.cc common_ind.hpp
K_linked_regions_extensions_SOURCES=K_linked_regions_extensions.cc common_ind.hpp
K_linked_regions_generalized_rec_SOURCES=K_linked_regions_generalized_rec.cc common_ind.hpp
HOC_ind_SOURCES=HOC_ind.cc

AM_CPPFLAGS=-Wall -W -I.

AM_CXXFLAGS=
if HAVE_SIMDATA_HPP
if HAVE_LIBSEQ_RUNTIME
AM_CXXFLAGS+=-DHAVE_LIBSEQUENCE
AM_LIBS=-lsequence
endif
endif
if DEBUG
else !DEBUG
AM_CPPFLAGS+=-DNDEBUG
endif
LDADD=
LIBS+=$(AM_LIBS)
