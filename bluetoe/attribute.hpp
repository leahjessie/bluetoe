#ifndef BLUETOE_ATTRIBUTE_HPP
#define BLUETOE_ATTRIBUTE_HPP

#include <bluetoe/options.hpp>
#include <bluetoe/client_characteristic_configuration.hpp>
#include <cstdint>
#include <cstddef>
#include <cassert>

namespace bluetoe {
namespace details {

    /*
     * Attribute and accessing an attribute
     */

    enum class attribute_access_result {
        success,
        // read just as much as was possible to write into the output buffer
        read_truncated,
        // the data to be written was larger than the attribute can store
        write_overflow,
        // the read/write offset is greater than attributes data size
        invalid_offset,

        write_not_permitted,
        read_not_permitted,

        // returned when access type is compare_128bit_uuid and the attribute contains a 128bit uuid and
        // the buffer in attribute_access_arguments is equal to the contained uuid.
        uuid_equal,
        value_equal
    };

    enum class attribute_access_type {
        read,
        write,
        compare_128bit_uuid,
        compare_value
    };

    struct attribute_access_arguments
    {
        attribute_access_type               type;
        std::uint8_t*                       buffer;
        std::size_t                         buffer_size;
        std::size_t                         buffer_offset;
        client_characteristic_configuration client_config;

        template < std::size_t N >
        static attribute_access_arguments read( std::uint8_t(&buffer)[N], std::size_t offset , const client_characteristic_configuration& cc = client_characteristic_configuration())
        {
            return attribute_access_arguments{
                attribute_access_type::read,
                &buffer[ 0 ],
                N,
                offset,
                cc
            };
        }

        static attribute_access_arguments read( std::uint8_t* begin, std::uint8_t* end, std::size_t offset, const client_characteristic_configuration& cc )
        {
            assert( end >= begin );

            return attribute_access_arguments{
                attribute_access_type::read,
                begin,
                static_cast< std::size_t >( end - begin ),
                offset,
                cc
            };
        }

        template < std::size_t N >
        static attribute_access_arguments write( const std::uint8_t(&buffer)[N], std::size_t offset = 0, const client_characteristic_configuration& cc = client_characteristic_configuration() )
        {
            return attribute_access_arguments{
                attribute_access_type::write,
                const_cast< std::uint8_t* >( &buffer[ 0 ] ),
                N,
                offset,
                cc
            };
        }

        static attribute_access_arguments write( const std::uint8_t* begin, const std::uint8_t* end, std::size_t offset, const client_characteristic_configuration& cc )
        {
            assert( end >= begin );

            return attribute_access_arguments{
                attribute_access_type::write,
                const_cast< std::uint8_t* >( begin ),
                static_cast< std::size_t >( end - begin ),
                offset,
                cc
            };
        }

        static attribute_access_arguments check_write()
        {
            return attribute_access_arguments{
                attribute_access_type::write,
                0,
                0,
                0,
                client_characteristic_configuration()
            };
        }

        static attribute_access_arguments compare_128bit_uuid( const std::uint8_t* uuid )
        {
            return attribute_access_arguments{
                attribute_access_type::compare_128bit_uuid,
                const_cast< std::uint8_t* >( uuid ),
                16u,
                0,
                client_characteristic_configuration()
            };
        }

        static attribute_access_arguments compare_value( const std::uint8_t* begin, const std::uint8_t* end )
        {
            assert( end >= begin );

            return attribute_access_arguments{
                attribute_access_type::compare_value,
                const_cast< std::uint8_t* >( begin ),
                static_cast< std::size_t >( end - begin ),
                0,
                client_characteristic_configuration()
            };
        }
    };

    typedef attribute_access_result ( *attribute_access )( attribute_access_arguments&, std::uint16_t attribute_handle );

    /*
     * An attribute is an uuid combined with a mean of how to access the attributes
     *
     * design decisions to _not_ use pointer to staticaly allocated virtual objects:
     * - makeing access a pointer to a virtual base class would result in storing a pointer that points to a pointer (vtable) to list with a bunch of functions.
     * - it's most likely that most attributes will not have any mutable data.
     * attribute contains only one function that takes a attribute_access_type to save memory and in the expectation that there are only a few different combination of
     * access functions.
     */
    struct attribute {
        // all uuids used by GATT are 16 bit UUIDs (except for Characteristic Value Declaration for which the value internal_128bit_uuid is used, if the UUID is 128bit long)
        std::uint16_t       uuid;
        attribute_access    access;
    };

    /*
     * Given that T is a tuple with elements that implement attribute_at< std::size_t, ServiceUUID >() and number_of_attributes, the type implements
     * attribute_at() for a list of attribute lists.
     */
    template < typename T, std::size_t ClientCharacteristicIndex, typename ServiceUUID >
    struct attribute_at_list;

    template < std::size_t ClientCharacteristicIndex, typename ServiceUUID >
    struct attribute_at_list< std::tuple<>, ClientCharacteristicIndex, ServiceUUID >
    {
        static details::attribute attribute_at( std::size_t index )
        {
            assert( !"index out of bound" );
            return details::attribute();
        }
    };

    template <
        typename T,
        typename ...Ts,
        std::size_t ClientCharacteristicIndex,
        typename ServiceUUID >
    struct attribute_at_list< std::tuple< T, Ts... >, ClientCharacteristicIndex, ServiceUUID >
    {
        static details::attribute attribute_at( std::size_t index )
        {
            if ( index < T::number_of_attributes )
                return T::template attribute_at< ClientCharacteristicIndex, ServiceUUID >( index );

            typedef details::attribute_at_list< std::tuple< Ts... >, ClientCharacteristicIndex + T::number_of_client_configs, ServiceUUID > remaining_characteristics;

            return remaining_characteristics::attribute_at( index - T::number_of_attributes );
        }
    };

    /*
     * Iterating the list of services is the same, but needs less parameters
     */
    template < typename Serives, std::size_t ClientCharacteristicIndex = 0 >
    struct attribute_from_service_list;

    template < std::size_t ClientCharacteristicIndex >
    struct attribute_from_service_list< std::tuple<>, ClientCharacteristicIndex >
    {
        static details::attribute attribute_at( std::size_t index )
        {
            assert( !"index out of bound" );
            return details::attribute();
        }
    };

    template <
        typename T,
        typename ...Ts,
        std::size_t ClientCharacteristicIndex >
    struct attribute_from_service_list< std::tuple< T, Ts... >, ClientCharacteristicIndex >
    {
        static details::attribute attribute_at( std::size_t index )
        {
            if ( index < T::number_of_attributes )
                return T::template attribute_at< ClientCharacteristicIndex >( index );

            typedef details::attribute_from_service_list< std::tuple< Ts... >, ClientCharacteristicIndex + T::number_of_client_configs > remaining_characteristics;

            return remaining_characteristics::attribute_at( index - T::number_of_attributes );
        }
    };

    /**
     * @brief data needed to send an indication or notification to the l2cap layer
     */
    class notification_data
    {
    public:
        notification_data()
            : att_handle_( 0 )
            , characteristic_value_attribute_{ 0, nullptr }
            , client_characteristic_configuration_index_( 0 )
        {
            assert( !valid() );
        }

        notification_data( std::uint16_t value_attribute_handle, const attribute& value_attribute, std::size_t client_characteristic_configuration_index )
            : att_handle_( value_attribute_handle )
            , characteristic_value_attribute_( value_attribute )
            , client_characteristic_configuration_index_( client_characteristic_configuration_index )
        {
            assert( valid() );
        }

        bool valid() const
        {
            return att_handle_ != 0;
        }

        std::uint16_t handle() const
        {
            return att_handle_;
        }

        const attribute& value_attribute() const
        {
            return characteristic_value_attribute_;
        }

        std::size_t client_characteristic_configuration_index() const
        {
            return client_characteristic_configuration_index_;
        }
    private:
        std::uint16_t   att_handle_;
        attribute       characteristic_value_attribute_;
        std::size_t     client_characteristic_configuration_index_;
    };

    /*
     * similar to the algorithm above, but this time the number of attributes is summed up.
     */
    template < typename T >
    struct find_notification_data_in_list;

    template <>
    struct find_notification_data_in_list< std::tuple<> >
    {
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static notification_data find_notification_data( const void* )
        {
            return notification_data();
        }
    };

    template <
        typename T,
        typename ...Ts
    >
    struct find_notification_data_in_list< std::tuple< T, Ts... > >
    {
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static notification_data find_notification_data( const void* value )
        {
            notification_data result = T::template find_notification_data< FirstAttributesHandle, ClientCharacteristicIndex >( value );

            if ( !result.valid() )
            {
                typedef find_notification_data_in_list< std::tuple< Ts... > > next;
                result = next::template find_notification_data<
                    FirstAttributesHandle + T::number_of_attributes,
                    ClientCharacteristicIndex + T::number_of_client_configs >( value );
            }

            return result;
        }
    };
}
}

#endif