/*
  \file extensions.cc
  API checks on fwdpp's extensions sub-library.
*/

#include <config.h>
#include <boost/test/unit_test.hpp>
#include <fwdpp/sugar/GSLrng_t.hpp>
#include <fwdpp/extensions/regions.hpp>
#include <fwdpp/type_traits.hpp>
#include <limits>
#include "../fixtures/sugar_fixtures.hpp"

using namespace fwdpp;

using poptype = singlepop_popgenmut_fixture::poptype;
BOOST_FIXTURE_TEST_SUITE(test_extensions, singlepop_popgenmut_fixture)

// Check that extensions::discrete_mut_model::operator() compiles
BOOST_AUTO_TEST_CASE(discrete_mut_model_test_1)
{
    // attempt
    extensions::discrete_mut_model dm({ 0, 1 }, { 1, 2 }, { 1, 0.5 }, {}, {},
                                      {}, {});
    // Check copy-constructible:
    decltype(dm) dm2(dm);
    // move-constructible:
    decltype(dm) dm3(std::move(dm2));
    auto rb = fwdpp_internal::make_mut_queue(pop.mcounts);
    fwdpp::GSLrng_t<fwdpp::GSL_RNG_TAUS2> rng(0u);
    auto x = dm(rb, pop.mutations, rng.get(), 0.001, 0., &generation,
                pop.mut_lookup);
    static_assert(std::is_same<decltype(x), std::size_t>::value,
                  "extensions::discrete_mut_model::operator() must return a "
                  "std::size_t");
}
// Check that extensions::discrete_mut_model::operator() can be bound
BOOST_AUTO_TEST_CASE(discrete_mut_model_test_2)
{
    // attempt
    extensions::discrete_mut_model dm({ 0, 1 }, { 1, 2 }, { 1, 0.5 }, {}, {},
                                      {}, {});
    auto rb = fwdpp_internal::make_mut_queue(pop.mcounts);
    fwdpp::GSLrng_t<fwdpp::GSL_RNG_TAUS2> rng(0u);
    auto mmodel = std::bind(
        &extensions::discrete_mut_model::
        operator()<fwdpp::traits::recycling_bin_t<decltype(pop.mutations)>,
                   decltype(pop.mut_lookup), decltype(pop.mutations)>,
        &dm, rb, pop.mutations, rng.get(), 0.001, 0., &generation,
        std::ref(pop.mut_lookup));
    auto x = mmodel();
    static_assert(
        std::is_same<decltype(x), std::size_t>::value,
        "extensions::discrete_mut_model::make_muts must return a std::size_t");
}

// Check that extensions::discrete_mut_model::make_mut can be bound
// with placeholders, and that the resulting type is a valid
// mutation model
BOOST_AUTO_TEST_CASE(discrete_mut_model_test_3)
{
    // attempt
    extensions::discrete_mut_model dm({ 0, 1 }, { 1, 2 }, { 1, 0.5 }, {}, {},
                                      {}, {});
    auto rb = fwdpp_internal::make_mut_queue(pop.mcounts);
    fwdpp::GSLrng_t<fwdpp::GSL_RNG_TAUS2> rng(0u);
    auto mmodel = std::bind(
        &extensions::discrete_mut_model::
        operator()<fwdpp::traits::recycling_bin_t<decltype(pop.mutations)>,
                   decltype(pop.mut_lookup), decltype(pop.mutations)>,
        &dm, std::placeholders::_1, std::placeholders::_2, rng.get(), 0.001,
        0., &generation, std::ref(pop.mut_lookup));
    static_assert(
        traits::is_mutation_model<decltype(mmodel), poptype::mcont_t,
                                  poptype::gcont_t>::value,
        "error: type mutation_model is not a dispatchable mutation model "
        "type!");
    auto x = mmodel(rb, pop.mutations);
    static_assert(
        std::is_same<decltype(x), std::size_t>::value,
        "extensions::discrete_mut_model::make_muts must return a std::size_t");
}

// check return type of extensions::discrete_rec_model
BOOST_AUTO_TEST_CASE(discrete_rec_model_test_1)
{
    // use really big recombination rate here to ensure that return value is
    // not empty
    extensions::discrete_rec_model drm(rng.get(), 50., { 0, 1 }, { 1, 2 },
                                       { 1, 2 });
    fwdpp::GSLrng_t<fwdpp::GSL_RNG_TAUS2> rng(0u);
    auto x = drm();
    static_assert(std::is_same<decltype(x), std::vector<double>>::value,
                  "extensions::dicrete_rec_model::operator() must return "
                  "std::vector<double>");
    BOOST_REQUIRE(x.empty()
                  || (x.back() == std::numeric_limits<double>::max()));
}

BOOST_AUTO_TEST_CASE(discrete_rec_model_pass_as_fxn)
{
    const auto f = [](const std::function<std::vector<double>()> & )
    {
    };

    extensions::discrete_rec_model drm(rng.get(), 50., { 0, 1 }, { 1, 2 },
                                       { 1, 2 });
    f(drm);
}

BOOST_AUTO_TEST_CASE(bound_drm_is_recmodel)
{
    extensions::discrete_rec_model drm(rng.get(), 1e-3, { 0, 1 }, { 1, 2 },
                                       { 1, 1 });
    static_assert(std::is_convertible<extensions::discrete_rec_model,
            std::function<std::vector<double>()>>::value,
            "extensions::discrete_rec_model must be convertible to std::function");

    static_assert(
        fwdpp::traits::
            is_rec_model<decltype(drm),
                         singlepop_popgenmut_fixture::poptype::diploid_t,
                         singlepop_popgenmut_fixture::poptype::gamete_t,
                         singlepop_popgenmut_fixture::poptype::mcont_t>::value,
        "bound object must be valid recombination model");
}

// Tests of raising exceptions
BOOST_AUTO_TEST_CASE(discrete_rec_model_constructor_should_throw)
{
    {
        BOOST_REQUIRE_THROW(extensions::discrete_rec_model drm(
                                rng.get(), 1e-3, { 0 }, { 1, 2 }, { 1, 2 }),
                            std::invalid_argument);
    }
    {
        BOOST_REQUIRE_THROW(extensions::discrete_rec_model drm(
                                rng.get(), 1e-3, { 0, 1 }, { 1 }, { 1, 2 }),
                            std::invalid_argument);
    }
    {
        BOOST_REQUIRE_THROW(
            extensions::discrete_rec_model drm(rng.get(), 1e-3, { 0, 1 },
                                               { 1, 2 }, { 1, 2, 3 }),
            std::invalid_argument);
    }
}

BOOST_AUTO_TEST_CASE(discrete_mut_model_constructor_should_throw)
{
    {
        BOOST_REQUIRE_THROW(extensions::discrete_mut_model dm(
                                { 0, 1 }, { 1, 2 }, { 1 }, {}, {}, {}, {}),
                            std::invalid_argument);
    }
    {
        BOOST_REQUIRE_THROW(
            extensions::discrete_mut_model dm({ 0, 1 }, { 1, 2 }, { 1, 2 },
                                              // incorrect number of weights
                                              { 0, 1 }, { 1, 2 }, { 1 }, {}),
            std::invalid_argument);
    }
    {
        BOOST_REQUIRE_THROW(
            extensions::discrete_mut_model dm(
                { 0, 1 }, { 1, 2 }, { 1, 2 },
                // There are selected regions, but no "sh models"
                { 0, 1 }, { 1, 2 }, { 1 }, {}),
            std::invalid_argument);
    }
}

BOOST_AUTO_TEST_SUITE_END()

