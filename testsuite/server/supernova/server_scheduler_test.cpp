#include <boost/test/unit_test.hpp>

#include <thread>

#include "server/server_scheduler.hpp"

using namespace nova;
using namespace boost;

// commented out due to switching from boost::thread to std::thread which does
// not include barrier.
// When switching to c++ 20 use https://en.cppreference.com/w/cpp/thread/barrier

BOOST_AUTO_TEST_CASE(scheduler_test_1) {
    scheduler<> sched(1);
    /*     sched(); */
}

namespace {

// boost::barrier barr(2);
// void thread_fn(scheduler<>* sched) {
//     for (int i = 0; i != 1000; ++i)
//         /* (*sched)() */;
//     barr.wait();
// }
}

// BOOST_AUTO_TEST_CASE(scheduler_test_2) {
//     scheduler<> sched(1);
//     std::thread thrd(std::bind(thread_fn, &sched));
//     barr.wait();
//     thrd.join();
// }
