#include <doctest/doctest.h>
#include <agrobus/isobus/vt/objects.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;

TEST_CASE("VTObject") {
    SUBCASE("serialize with body") {
        VTObject obj;
        obj.id = 100;
        obj.type = ObjectType::Button;
        obj.body = {0x10, 0x20, 0x30}; // Some object-specific data

        auto data = obj.serialize();
        // Header: 2 (id) + 1 (type) + 2 (body_len) + 3 (body) = 8
        CHECK(data.size() == 8);
        // Check object ID (LE)
        CHECK(data[0] == 100);
        CHECK(data[1] == 0);
        // Check type
        CHECK(data[2] == static_cast<u8>(ObjectType::Button));
        // Check body length (LE) = 3
        CHECK(data[3] == 3);
        CHECK(data[4] == 0);
        // Check body data
        CHECK(data[5] == 0x10);
        CHECK(data[6] == 0x20);
        CHECK(data[7] == 0x30);
    }

    SUBCASE("serialize with children") {
        VTObject obj;
        obj.id = 1;
        obj.type = ObjectType::DataMask;
        obj.body = {0xAA};
        obj.children = {10, 20};

        auto data = obj.serialize();
        // Header: 2+1+2 = 5, body: 1, children: 2 (count) + 4 (2 IDs) = 6, total body_len = 7
        CHECK(data.size() == 12);
        // Body length = 1 + 6 = 7
        CHECK(data[3] == 7);
        CHECK(data[4] == 0);
        // Body byte
        CHECK(data[5] == 0xAA);
        // Children count
        CHECK(data[6] == 2);
        CHECK(data[7] == 0);
        // First child ID = 10
        CHECK(data[8] == 10);
        CHECK(data[9] == 0);
        // Second child ID = 20
        CHECK(data[10] == 20);
        CHECK(data[11] == 0);
    }

    SUBCASE("serialize empty body no children") {
        VTObject obj;
        obj.id = 5;
        obj.type = ObjectType::NumberVariable;

        auto data = obj.serialize();
        // 2+1+2+0 = 5
        CHECK(data.size() == 5);
        CHECK(data[3] == 0); // body_len = 0
        CHECK(data[4] == 0);
    }
}

TEST_CASE("ObjectPool") {
    ObjectPool pool;

    SUBCASE("add objects") {
        VTObject obj1;
        obj1.id = 1;
        obj1.type = ObjectType::DataMask;
        auto r1 = pool.add(std::move(obj1));
        CHECK(r1.is_ok());
        CHECK(pool.size() == 1);
    }

    SUBCASE("reject duplicate ID") {
        VTObject obj1;
        obj1.id = 1;
        pool.add(std::move(obj1));

        VTObject obj2;
        obj2.id = 1; // Same ID
        auto result = pool.add(std::move(obj2));
        CHECK(result.is_err());
    }

    SUBCASE("find by ID") {
        VTObject obj;
        obj.id = 42;
        obj.type = ObjectType::Button;
        pool.add(std::move(obj));

        auto found = pool.find(42);
        CHECK(found.has_value());
        CHECK((*found)->type == ObjectType::Button);

        auto not_found = pool.find(99);
        CHECK(!not_found.has_value());
    }

    SUBCASE("serialize and deserialize pool") {
        VTObject obj1;
        obj1.id = 1;
        obj1.type = ObjectType::WorkingSet;
        obj1.body = {0x01, 0x02};
        pool.add(std::move(obj1));

        VTObject obj2;
        obj2.id = 2;
        obj2.type = ObjectType::DataMask;
        obj2.body = {0x03, 0x04, 0x05};
        pool.add(std::move(obj2));

        auto ser_result = pool.serialize();
        CHECK(ser_result.is_ok());
        CHECK(ser_result.value().size() > 0);

        // Deserialize and verify round-trip
        auto deser_result = ObjectPool::deserialize(ser_result.value());
        CHECK(deser_result.is_ok());
        auto &pool2 = deser_result.value();
        CHECK(pool2.size() == 2);

        auto found1 = pool2.find(1);
        CHECK(found1.has_value());
        CHECK((*found1)->type == ObjectType::WorkingSet);
        CHECK((*found1)->body.size() == 2);
        CHECK((*found1)->body[0] == 0x01);

        auto found2 = pool2.find(2);
        CHECK(found2.has_value());
        CHECK((*found2)->type == ObjectType::DataMask);
        CHECK((*found2)->body.size() == 3);
    }

    SUBCASE("deserialize truncated data") {
        dp::Vector<u8> bad_data = {0x01, 0x00, 0x00, 0xFF, 0x00}; // body_len=255 but no data
        auto result = ObjectPool::deserialize(bad_data);
        CHECK(result.is_err());
    }

    SUBCASE("version label") {
        pool.set_version_label("v1.0");
        CHECK(pool.version_label() == "v1.0");
    }

    SUBCASE("clear pool") {
        VTObject obj;
        obj.id = 1;
        pool.add(std::move(obj));
        CHECK(!pool.empty());

        pool.clear();
        CHECK(pool.empty());
        CHECK(pool.size() == 0);
    }
}
