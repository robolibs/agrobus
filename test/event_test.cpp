#include <doctest/doctest.h>
#include <agrobus/net/event.hpp>

using namespace agrobus::net;

TEST_CASE("Event subscribe/emit") {
    SUBCASE("single listener") {
        Event<i32> event;
        i32 received = 0;
        event.subscribe([&](i32 val) { received = val; });
        event.emit(42);
        CHECK(received == 42);
    }

    SUBCASE("multiple listeners") {
        Event<i32> event;
        i32 sum = 0;
        event.subscribe([&](i32 val) { sum += val; });
        event.subscribe([&](i32 val) { sum += val * 2; });
        event.emit(10);
        CHECK(sum == 30); // 10 + 20
    }

    SUBCASE("multiple arguments") {
        Event<i32, f32> event;
        i32 i_val = 0;
        f32 f_val = 0;
        event.subscribe([&](i32 i, f32 f) { i_val = i; f_val = f; });
        event.emit(5, 3.14f);
        CHECK(i_val == 5);
        CHECK(f_val == doctest::Approx(3.14f));
    }

    SUBCASE("no arguments") {
        Event<> event;
        i32 count = 0;
        event.subscribe([&]() { count++; });
        event.emit();
        event.emit();
        CHECK(count == 2);
    }

    SUBCASE("count listeners") {
        Event<i32> event;
        CHECK(event.count() == 0);
        event.subscribe([](i32) {});
        CHECK(event.count() == 1);
        event.subscribe([](i32) {});
        CHECK(event.count() == 2);
    }

    SUBCASE("clear listeners") {
        Event<i32> event;
        i32 val = 0;
        event.subscribe([&](i32 v) { val = v; });
        event.clear();
        event.emit(99);
        CHECK(val == 0);
        CHECK(event.count() == 0);
    }

    SUBCASE("operator+=") {
        Event<i32> event;
        i32 val = 0;
        event += [&](i32 v) { val = v; };
        event.emit(7);
        CHECK(val == 7);
    }
}
