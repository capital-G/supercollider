#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>

#include <chrono>
#include <thread>

#define protected public

#include "audio_backend/audio_backend.hpp"
#include "audio_backend/portaudio.hpp"

using namespace nova;

namespace {
void tick(void) {}
}

template <typename backend> void test_backend(void) {
    backend be;

    BOOST_REQUIRE(!be.audiostream_ready());

    device_list devs = be.list_devices();
    be.open_audio_stream(devs[0], devs[0].inchannels, devs[0], devs[0].outchannels, devs[0].defaultSampleRate);

    BOOST_REQUIRE(be.audiostream_ready());

    BOOST_REQUIRE(!be.is_active());
    be.activate();
    BOOST_REQUIRE(be.is_active());

    auto now = std::chrono::high_resolution_clock::now();
    auto future_time = now + std::chrono::milliseconds(1000);

    std::this_thread::sleep_until(future_time);

    be.deactivate();
    BOOST_REQUIRE(!be.is_active());
    be.close_audio_stream();
    BOOST_REQUIRE(!be.audiostream_ready());
}

BOOST_AUTO_TEST_CASE(dummy_test_1) { test_backend<dummy_backend<&tick>>(); }

BOOST_AUTO_TEST_CASE(pa_test_1) { test_backend<portaudio_backend<&tick>>(); }
