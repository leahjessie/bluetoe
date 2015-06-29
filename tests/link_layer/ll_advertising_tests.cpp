#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/server.hpp>
#include "test_radio.hpp"
#include "../test_servers.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <map>

namespace {

    struct advertising : bluetoe::link_layer::link_layer< small_temperature_service, test::radio >
    {
        advertising()
        {
            this->run( gatt_server_ );
        }

        small_temperature_service gatt_server_;
    };

}

BOOST_FIXTURE_TEST_CASE( advertising_scheduled, advertising )
{
    BOOST_CHECK_GT( scheduling().size(), 0 );
}

BOOST_FIXTURE_TEST_CASE( advertising_uses_all_three_adv_channels, advertising )
{
    std::map< unsigned, unsigned > channel;

    all_data( [&]( const test::schedule_data& d ) { ++channel[ d.channel ]; } );

    BOOST_CHECK_EQUAL( channel.size(), 3u );
    BOOST_CHECK_GT( channel[ 37 ], 0 );
    BOOST_CHECK_GT( channel[ 38 ], 0 );
    BOOST_CHECK_GT( channel[ 39 ], 0 );
}

BOOST_FIXTURE_TEST_CASE( channels_are_iterated, advertising )
{
    check_scheduling(
        [&]( const test::schedule_data& a, const test::schedule_data& b )
        {
            return a.channel != b.channel;
        },
        "channels_are_iterated"
    );
}

/**
 * @test by default, the link layer will advertise Connectable Undirected Events
 */
BOOST_FIXTURE_TEST_CASE( connectable_undirected_is_the_default, advertising )
{
    check_scheduling(
        [&]( const test::schedule_data& scheduled ) -> bool
        {
            return !scheduled.transmitted_data.empty()
                && ( scheduled.transmitted_data[ 0 ] & 0xF0 ) == 0;
        },
        "channels_are_iterated"
    );
}

/**
 * @test according to the specs, the distance between two advertising PDUs shall be short than or equal to 10ms
 *       We identify the start of an advertising event by the first channel 37
 */
BOOST_FIXTURE_TEST_CASE( less_than_10ms_between_two_PDUs, advertising )
{
    check_scheduling(
        [&]( const test::schedule_data& a, const test::schedule_data& b )
        {
            return a.channel != 37 && a.channel != 38
                || ( b.transmision_time - a.transmision_time ) <= bluetoe::link_layer::delta_time::msec( 10 );
        },
        "less_than_10ms_between_two_PDUs"
    );
}