#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/options.hpp>

#include <cstddef>
#include <cassert>
#include <algorithm>
#include <type_traits>

namespace bluetoe {

    namespace details {
        struct characteristic_meta_type;
        struct characteristic_uuid_meta_type;
        struct characteristic_value_metat_type;
    }

    /**
     * @brief a 128-Bit UUID used to identify a characteristic.
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * @code{.cpp}
     * bluetoe::characteristic_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     * @endcode
     */
    template <
        std::uint64_t A,
        std::uint64_t B,
        std::uint64_t C,
        std::uint64_t D,
        std::uint64_t E >
    struct characteristic_uuid : details::uuid< A, B, C, D, E >
    {
        typedef details::characteristic_uuid_meta_type meta_type;
    };

    /**
     * @brief a 16-Bit UUID used to identify a characteristic.
     */
    template <
        std::uint64_t UUID >
    struct characteristic_uuid16 : details::uuid16< UUID >
    {
        typedef details::characteristic_uuid_meta_type meta_type;
        static constexpr bool is_128bit = false;
    };

    /**
     * @brief a characteristic is a data point that is accessable by client.
     */
    template < typename ... Options >
    class characteristic
    {
    public:
        /**
         * a service is a list of attributes
         */
        static constexpr std::size_t number_of_attributes = 2;

        /**
         * @brief gives access to the all attributes of the characteristic
         *
         * @TODO: the "Characteristic Declaration" contains an absolute handle value of the "Characteristic Value"
         *        that "Characteristic Value" is the first attribute behind the Declaration. Two possible solutions:
         *        - extend attribute_at to take two index (a global one) and a local one.
         *        - return a wrong handle and fix the handle on a higher level
         *
         * @TODO: the "Characteristic Value Declaration" has an attribute type that can be 16 or 128 bit.
         */
        static details::attribute attribute_at( std::size_t index );

        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type uuid;
        typedef details::characteristic_meta_type meta_type;

    private:
        typedef typename details::find_by_meta_type< details::characteristic_value_metat_type, Options... >::type base_value_type;
        typedef typename base_value_type::template value_impl< Options... >                                       value_type;

        static details::attribute_access_result char_declaration_access( details::attribute_access_arguments& );

    };

    /**
     * @brief if added as option to a characteristic, read access is removed from the characteristic
     *
     * Even if read access was the only remaining access type, the characterist will not be readable.
     *
     * Example:
     * @code
        std::uint32_t simple_value = 0xaabbccdd;

        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_read_access > >
     * @endcode
     * @sa characteristic
     */
    class no_read_access {};

    /**
     * @brief if added as option to a characteristic, write access is removed from the characteristic
     *
     * Even if write access was the only remaining access type, the characterist will not be writeable.
     *
     * Example:
     * @code
        std::uint32_t simple_value = 0xaabbccdd;

        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_write_access > >
     * @endcode
     * @sa characteristic
     */
    class no_write_access {};

    /**
     * @brief a very simple device to bind a characteristic to a global variable to provide access to the characteristic value
     */
    template < typename T, T* Ptr >
    class bind_characteristic_value
    {
    public:
        // use a new type to mixin the options given to characteristic
        template < typename ... Options >
        class value_impl
        {
        public:
            static constexpr bool has_read_access  = !details::has_option< no_read_access, Options... >::value;
            static constexpr bool has_write_access = !std::is_const< T >::value && !details::has_option< no_write_access, Options... >::value;

            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& );
        private:
            static details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments&, const std::true_type& );
            static details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments&, const std::false_type& );
            static details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments&, const std::true_type& );
            static details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments&, const std::false_type& );
        };


        typedef details::characteristic_value_metat_type meta_type;
    };

    /**
     * @brief adds a name to characteristic
     *
     * Adds a "Characteristic User Description" to the characteristic
     */
    template < const char* const >
    class characteristic_name
    {
    };

    // implementation
    template < typename ... Options >
    details::attribute characteristic< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        static const details::attribute attributes[ number_of_attributes ] = {
            { bits( details::gatt_uuids::characteristic ), &char_declaration_access },
            {
                uuid::is_128bit
                    ? bits( details::gatt_uuids::internal_128bit_uuid )
                    : uuid::as_16bit(),
                &value_type::characteristic_value_access
            }
        };

        return attributes[ index ];
    }

    template < typename ... Options >
    details::attribute_access_result characteristic< Options... >::char_declaration_access( details::attribute_access_arguments& args )
    {
        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type uuid;
        static const auto uuid_offset = 3;

        if ( args.type == details::attribute_access_type::read )
        {
            static constexpr auto uuid_size = sizeof( uuid::bytes );

            args.buffer_size          = std::min< std::size_t >( args.buffer_size, uuid_offset + uuid_size );
            const auto max_uuid_bytes = std::min< std::size_t >( std::max< int >( 0, args.buffer_size -uuid_offset ), uuid_size );

            if ( args.buffer_size > 0 )
            {
                args.buffer[ 0 ] =
                    ( value_type::has_read_access  ? bits( details::gatt_characteristic_properties::read ) : 0 ) |
                    ( value_type::has_write_access ? bits( details::gatt_characteristic_properties::write ) : 0 );
            }

            /// @TODO fill "Characteristic Value Attribute Handle"
            if ( max_uuid_bytes )
                std::copy( std::begin( uuid::bytes ), std::begin( uuid::bytes ) + max_uuid_bytes, args.buffer + uuid_offset );

            return args.buffer_size == uuid_offset + uuid_size
                ? details::attribute_access_result::success
                : details::attribute_access_result::read_truncated;
        }

        return details::attribute_access_result::write_not_permitted;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_access( details::attribute_access_arguments& args )
    {
        if ( args.type == details::attribute_access_type::read )
        {
            return characteristic_value_read_access( args, std::integral_constant< bool, has_read_access >() );
        }
        else if ( args.type == details::attribute_access_type::write )
        {
            return characteristic_value_write_access( args, std::integral_constant< bool, has_write_access >() );
        }

        return details::attribute_access_result::write_not_permitted;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_read_access( details::attribute_access_arguments& args, const std::true_type& )
    {
        args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) );
        static const std::uint8_t* ptr = static_cast< const std::uint8_t* >( static_cast< const void* >( Ptr ) );
        std::copy( ptr, ptr + args.buffer_size, args.buffer );

        return args.buffer_size == sizeof( T )
            ? details::attribute_access_result::success
            : details::attribute_access_result::read_truncated;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_read_access( details::attribute_access_arguments&, const std::false_type& )
    {
        return details::attribute_access_result::read_not_permitted;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_write_access( details::attribute_access_arguments& args, const std::true_type& )
    {
        args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) );
        static std::uint8_t* ptr = static_cast< std::uint8_t* >( static_cast< void* >( Ptr ) );
        std::copy( args.buffer, args.buffer + args.buffer_size, ptr );

        return args.buffer_size == sizeof( T )
            ? details::attribute_access_result::success
            : details::attribute_access_result::write_truncated;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_write_access( details::attribute_access_arguments&, const std::false_type& )
    {
        return details::attribute_access_result::write_not_permitted;
    }
}

#endif
