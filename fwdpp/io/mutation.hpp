#ifndef FWDPP_IO_MUTATION_HPP__
#define FWDPP_IO_MUTATION_HPP__

#include <cstddef>
#include <stdexcept>
#include <fwdpp/type_traits.hpp>
#include <fwdpp/forward_types.hpp>
#include "scalar_serialization.hpp"

/*! \namespace fwdpp::io
 * \brief I/O types and functions.
 *
 * Template typenames referring to streams assume input
 * or output streams compatible with those from namespace
 * std.
 *
 * See @ref md_md_serialization for details.
 */

namespace fwdpp
{
    namespace io
    {
        template <typename T> struct serialize_mutation
        /// \brief Serialize a mutation
        ///
        /// Serialize a mutation.
        /// This *must* be specialized for a specific mutation type,
        /// otherwise std::runtime_error will be thrown.
        /// The type T must be a valid mutation type.
        /// See serialize_mutation<popgenmut> for example implementation
        {
            template <typename istreamtype>
            inline void
            operator()(istreamtype &, const T &) const
            {
                throw std::runtime_error(
                    "serializtion not implemented for this mutation type");
            }
        };

        template <typename T> struct deserialize_mutation
        /// \brief Deserialize a mutation
        ///
        /// Deserialize a mutation.
        /// This *must* be specialized for a specific mutation type,
        /// otherwise std::runtime_error will be thrown.
        /// The type T must be a valid mutation type.
        /// See deserialize_mutation<popgenmut> for example implementation
        {
            template <typename ostreamtype>
            inline T
            operator()(ostreamtype &) const
            {
                throw std::runtime_error(
                    "deserializtion not implemented for this mutation type");
            }
        };

        template <typename mcont_t, typename ostreamtype>
        void
        write_mutations(ostreamtype &buffer, const mcont_t &mutations)
        /// \brief Serialize a container of mutations.
        ///
        /// Works via specialization of serialize_mutation.
        {
            static_assert(
                traits::is_mutation<typename mcont_t::value_type>::value,
                "mcont_t must be a container of mutations");
            std::size_t MUTNO = mutations.size();
            fwdpp::io::scalar_writer()(buffer, &MUTNO);
            // write the mutation data to the buffer
            fwdpp::io::serialize_mutation<typename mcont_t::value_type> mw;
            for (const auto &m : mutations)
                mw(buffer, m);
        }

        template <typename mcont_t, typename istreamtype>
        void
        read_mutations(istreamtype &in, mcont_t &mutations)
        /// \brief Deserialize a container of mutations.
        ///
        /// Works via specialization of deserialize_mutation.
        {
            static_assert(
                traits::is_mutation<typename mcont_t::value_type>::value,
                "mcont_t must be a container of mutations");
            std::size_t NMUTS;
            fwdpp::io::scalar_reader()(in, &NMUTS);
            fwdpp::io::deserialize_mutation<typename mcont_t::value_type> mr;
            for (uint_t i = 0; i < NMUTS; ++i)
                {
                    mutations.emplace_back(mr(in));
                }
        }
    }
}

#endif
