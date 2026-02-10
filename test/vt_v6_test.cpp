#include <doctest/doctest.h>
#include <agrobus.hpp>

using namespace agrobus::isobus;
using namespace agrobus::isobus::vt;
using namespace agrobus::net;

// ═════════════════════════════════════════════════════════════════════════════
// VT Version 6 Support Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("VTVersion enum includes Version6") {
    CHECK(static_cast<u8>(VTVersion::Version3) == 3);
    CHECK(static_cast<u8>(VTVersion::Version4) == 4);
    CHECK(static_cast<u8>(VTVersion::Version5) == 5);
    CHECK(static_cast<u8>(VTVersion::Version6) == 6);
}

TEST_CASE("VTClient Version 6 initialization") {
    IsoNet net;
    Name name = Name::build()
                    .set_identity_number(200)
                    .set_manufacturer_code(1)
                    .set_function_code(25)
                    .set_industry_group(2);
    auto cf = net.create_internal(name, 0, 0x40).value();

    VTClient vt(net, cf);

    SUBCASE("set VT version to 6") {
        vt.set_vt_version(VTVersion::Version6);
        // Version is set - would be used during connection handshake
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Touch Gesture Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("GestureType enum values") {
    CHECK(static_cast<u8>(GestureType::None) == 0);
    CHECK(static_cast<u8>(GestureType::Tap) == 1);
    CHECK(static_cast<u8>(GestureType::DoubleTap) == 2);
    CHECK(static_cast<u8>(GestureType::LongPress) == 3);
    CHECK(static_cast<u8>(GestureType::SwipeLeft) == 4);
    CHECK(static_cast<u8>(GestureType::SwipeRight) == 5);
    CHECK(static_cast<u8>(GestureType::SwipeUp) == 6);
    CHECK(static_cast<u8>(GestureType::SwipeDown) == 7);
    CHECK(static_cast<u8>(GestureType::PinchIn) == 8);
    CHECK(static_cast<u8>(GestureType::PinchOut) == 9);
    CHECK(static_cast<u8>(GestureType::Rotate) == 10);
    CHECK(static_cast<u8>(GestureType::TwoFingerTap) == 11);
    CHECK(static_cast<u8>(GestureType::ThreeFingerTap) == 12);
}

TEST_CASE("TouchGesture encode and decode - Tap") {
    TouchGesture gesture;
    gesture.type = GestureType::Tap;
    gesture.x = 320;
    gesture.y = 240;
    gesture.duration_ms = 150;
    gesture.touch_count = 1;
    gesture.target_object = 5000;

    auto encoded = gesture.encode();
    CHECK(encoded.size() >= 14);

    auto decoded = TouchGesture::decode(encoded);
    CHECK(decoded.is_ok());
    CHECK(decoded.value().type == GestureType::Tap);
    CHECK(decoded.value().x == 320);
    CHECK(decoded.value().y == 240);
    CHECK(decoded.value().duration_ms == 150);
    CHECK(decoded.value().touch_count == 1);
    CHECK(decoded.value().target_object == 5000);
}

TEST_CASE("TouchGesture encode and decode - DoubleTap") {
    TouchGesture gesture;
    gesture.type = GestureType::DoubleTap;
    gesture.x = 100;
    gesture.y = 50;
    gesture.duration_ms = 200;
    gesture.touch_count = 1;
    gesture.target_object = 3000;

    auto encoded = gesture.encode();
    auto decoded = TouchGesture::decode(encoded);

    CHECK(decoded.is_ok());
    CHECK(decoded.value().type == GestureType::DoubleTap);
    CHECK(decoded.value().x == 100);
    CHECK(decoded.value().y == 50);
}

TEST_CASE("TouchGesture encode and decode - LongPress") {
    TouchGesture gesture;
    gesture.type = GestureType::LongPress;
    gesture.x = 400;
    gesture.y = 300;
    gesture.duration_ms = 1500; // 1.5 seconds
    gesture.touch_count = 1;
    gesture.target_object = 7000;

    auto encoded = gesture.encode();
    auto decoded = TouchGesture::decode(encoded);

    CHECK(decoded.is_ok());
    CHECK(decoded.value().type == GestureType::LongPress);
    CHECK(decoded.value().duration_ms == 1500);
}

TEST_CASE("TouchGesture encode and decode - Swipe gestures") {
    SUBCASE("SwipeLeft") {
        TouchGesture gesture;
        gesture.type = GestureType::SwipeLeft;
        gesture.x = 640;
        gesture.y = 240;
        gesture.distance = -200; // Negative for left
        gesture.duration_ms = 300;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::SwipeLeft);
        CHECK(decoded.value().distance == -200);
    }

    SUBCASE("SwipeRight") {
        TouchGesture gesture;
        gesture.type = GestureType::SwipeRight;
        gesture.distance = 250;
        gesture.duration_ms = 250;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::SwipeRight);
        CHECK(decoded.value().distance == 250);
    }

    SUBCASE("SwipeUp") {
        TouchGesture gesture;
        gesture.type = GestureType::SwipeUp;
        gesture.distance = -150;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::SwipeUp);
    }

    SUBCASE("SwipeDown") {
        TouchGesture gesture;
        gesture.type = GestureType::SwipeDown;
        gesture.distance = 180;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::SwipeDown);
    }
}

TEST_CASE("TouchGesture encode and decode - Pinch gestures") {
    SUBCASE("PinchIn (zoom out)") {
        TouchGesture gesture;
        gesture.type = GestureType::PinchIn;
        gesture.x = 320;
        gesture.y = 240;
        gesture.scale = 0.5f; // 50% scale (zoom out)
        gesture.touch_count = 2;
        gesture.duration_ms = 500;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::PinchIn);
        CHECK(decoded.value().scale == doctest::Approx(0.5f).epsilon(0.01));
        CHECK(decoded.value().touch_count == 2);
    }

    SUBCASE("PinchOut (zoom in)") {
        TouchGesture gesture;
        gesture.type = GestureType::PinchOut;
        gesture.x = 400;
        gesture.y = 300;
        gesture.scale = 2.0f; // 200% scale (zoom in)
        gesture.touch_count = 2;
        gesture.duration_ms = 600;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::PinchOut);
        CHECK(decoded.value().scale == doctest::Approx(2.0f).epsilon(0.01));
    }
}

TEST_CASE("TouchGesture encode and decode - Rotate") {
    TouchGesture gesture;
    gesture.type = GestureType::Rotate;
    gesture.x = 320;
    gesture.y = 240;
    gesture.rotation_deg = 45.5f; // 45.5 degrees clockwise
    gesture.touch_count = 2;
    gesture.duration_ms = 800;

    auto encoded = gesture.encode();
    auto decoded = TouchGesture::decode(encoded);

    CHECK(decoded.is_ok());
    CHECK(decoded.value().type == GestureType::Rotate);
    CHECK(decoded.value().rotation_deg == doctest::Approx(45.5f).epsilon(0.1));
    CHECK(decoded.value().touch_count == 2);
}

TEST_CASE("TouchGesture encode and decode - Multi-finger taps") {
    SUBCASE("TwoFingerTap") {
        TouchGesture gesture;
        gesture.type = GestureType::TwoFingerTap;
        gesture.x = 200;
        gesture.y = 150;
        gesture.touch_count = 2;
        gesture.duration_ms = 100;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::TwoFingerTap);
        CHECK(decoded.value().touch_count == 2);
    }

    SUBCASE("ThreeFingerTap") {
        TouchGesture gesture;
        gesture.type = GestureType::ThreeFingerTap;
        gesture.x = 300;
        gesture.y = 200;
        gesture.touch_count = 3;
        gesture.duration_ms = 120;

        auto encoded = gesture.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().type == GestureType::ThreeFingerTap);
        CHECK(decoded.value().touch_count == 3);
    }
}

TEST_CASE("TouchGesture decode insufficient data") {
    dp::Vector<u8> short_data = {0x01, 0x00};
    auto decoded = TouchGesture::decode(short_data);
    CHECK(decoded.is_err());
}

// ═════════════════════════════════════════════════════════════════════════════
// VT6 Object Type Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("VT6 ObjectType enum values") {
    CHECK(static_cast<u8>(ObjectType::ExternalObjectDefinition) == 41);
    CHECK(static_cast<u8>(ObjectType::ExternalReferenceName) == 42);
    CHECK(static_cast<u8>(ObjectType::ExternalObjectPointer) == 43);
    CHECK(static_cast<u8>(ObjectType::ColourPalette) == 44);
    CHECK(static_cast<u8>(ObjectType::GraphicsContext) == 45);
    CHECK(static_cast<u8>(ObjectType::ObjectLabelRef) == 46);
    CHECK(static_cast<u8>(ObjectType::ScaledBitmap) == 47);
}

TEST_CASE("ExternalObjectDefinition encode and decode") {
    ExternalObjectDefinition ext_obj;
    ext_obj.name_id = 1000;
    ext_obj.default_object_id = 2000;

    auto encoded = ext_obj.encode();
    CHECK(encoded.size() >= 4);

    auto decoded = ExternalObjectDefinition::decode(encoded);
    CHECK(decoded.is_ok());
    CHECK(decoded.value().name_id == 1000);
    CHECK(decoded.value().default_object_id == 2000);
}

TEST_CASE("ExternalReferenceName encode and decode") {
    ExternalReferenceName ext_ref;
    ext_ref.name = "ISO11783-6:2018:VT6";

    auto encoded = ext_ref.encode();
    CHECK(encoded.size() >= ext_ref.name.size());

    auto decoded = ExternalReferenceName::decode(encoded);
    CHECK(decoded.is_ok());
    CHECK(decoded.value().name == "ISO11783-6:2018:VT6");
}

TEST_CASE("ExternalObjectPointer encode and decode") {
    ExternalObjectPointer ext_ptr;
    ext_ptr.name_id = 3000;
    ext_ptr.external_reference_name_id = 4000;
    ext_ptr.external_object_id = 5000;

    auto encoded = ext_ptr.encode();
    CHECK(encoded.size() >= 6);

    auto decoded = ExternalObjectPointer::decode(encoded);
    CHECK(decoded.is_ok());
    CHECK(decoded.value().name_id == 3000);
    CHECK(decoded.value().external_reference_name_id == 4000);
    CHECK(decoded.value().external_object_id == 5000);
}

// ═════════════════════════════════════════════════════════════════════════════
// 24-bit Graphics Context Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("GraphicsContextV6 encode and decode") {
    SUBCASE("basic graphics context") {
        GraphicsContextV6 ctx;
        ctx.transparency = 128; // 50% transparent
        ctx.line_style = 0;     // Solid
        ctx.line_width = 2;
        ctx.fill_color_rgb = 0xFF0000;  // Red
        ctx.line_color_rgb = 0x0000FF;  // Blue
        ctx.anti_aliasing = true;
        ctx.blend_mode = 0; // Normal

        auto encoded = ctx.encode();
        CHECK(encoded.size() >= 12);

        auto decoded = GraphicsContextV6::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().transparency == 128);
        CHECK(decoded.value().line_width == 2);
        CHECK(decoded.value().fill_color_rgb == 0xFF0000);
        CHECK(decoded.value().line_color_rgb == 0x0000FF);
        CHECK(decoded.value().anti_aliasing == true);
    }

    SUBCASE("fully opaque") {
        GraphicsContextV6 ctx;
        ctx.transparency = 0; // Fully opaque

        auto encoded = ctx.encode();
        auto decoded = GraphicsContextV6::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().transparency == 0);
    }

    SUBCASE("fully transparent") {
        GraphicsContextV6 ctx;
        ctx.transparency = 255; // Fully transparent

        auto encoded = ctx.encode();
        auto decoded = GraphicsContextV6::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().transparency == 255);
    }

    SUBCASE("different blend modes") {
        GraphicsContextV6 ctx;
        ctx.blend_mode = 1; // Multiply

        auto encoded = ctx.encode();
        auto decoded = GraphicsContextV6::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().blend_mode == 1);
    }
}

TEST_CASE("GraphicsContextV6 RGB color tests") {
    SUBCASE("pure colors") {
        GraphicsContextV6 ctx;

        // Red
        ctx.fill_color_rgb = 0xFF0000;
        CHECK(ctx.fill_color_rgb == 0xFF0000);

        // Green
        ctx.fill_color_rgb = 0x00FF00;
        CHECK(ctx.fill_color_rgb == 0x00FF00);

        // Blue
        ctx.fill_color_rgb = 0x0000FF;
        CHECK(ctx.fill_color_rgb == 0x0000FF);

        // White
        ctx.fill_color_rgb = 0xFFFFFF;
        CHECK(ctx.fill_color_rgb == 0xFFFFFF);

        // Black
        ctx.fill_color_rgb = 0x000000;
        CHECK(ctx.fill_color_rgb == 0x000000);
    }

    SUBCASE("custom RGB colors") {
        GraphicsContextV6 ctx;
        ctx.fill_color_rgb = 0x8B4513; // SaddleBrown
        ctx.line_color_rgb = 0x228B22; // ForestGreen

        auto encoded = ctx.encode();
        auto decoded = GraphicsContextV6::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().fill_color_rgb == 0x8B4513);
        CHECK(decoded.value().line_color_rgb == 0x228B22);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Colour Palette Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ColourPaletteV6 standard colors") {
    ColourPaletteV6 palette = ColourPaletteV6::standard_vt6_palette();

    CHECK(palette.colors.size() == 16);
    CHECK(palette.colors[0] == 0x000000);  // Black
    CHECK(palette.colors[1] == 0xFFFFFF);  // White
    CHECK(palette.colors[2] == 0x00FF00);  // Green
    CHECK(palette.colors[3] == 0x00A86B);  // Teal
    CHECK(palette.colors[4] == 0xFF0000);  // Red
    CHECK(palette.colors[15] == 0x808080); // Gray
}

TEST_CASE("ColourPaletteV6 encode and decode") {
    ColourPaletteV6 palette;
    palette.colors.push_back(0xFF0000); // Red
    palette.colors.push_back(0x00FF00); // Green
    palette.colors.push_back(0x0000FF); // Blue
    palette.colors.push_back(0xFFFF00); // Yellow

    auto encoded = palette.encode();
    CHECK(encoded.size() >= 4 * 3); // 4 colors * 3 bytes each

    auto decoded = ColourPaletteV6::decode(encoded);
    CHECK(decoded.is_ok());
    CHECK(decoded.value().colors.size() == 4);
    CHECK(decoded.value().colors[0] == 0xFF0000);
    CHECK(decoded.value().colors[1] == 0x00FF00);
    CHECK(decoded.value().colors[2] == 0x0000FF);
    CHECK(decoded.value().colors[3] == 0xFFFF00);
}

TEST_CASE("ColourPaletteV6 custom palette") {
    ColourPaletteV6 palette;

    // Create a grayscale palette
    for (u32 i = 0; i < 16; ++i) {
        u8 gray = i * 17; // 0, 17, 34, ..., 255
        u32 color = (gray << 16) | (gray << 8) | gray;
        palette.colors.push_back(color);
    }

    CHECK(palette.colors.size() == 16);
    CHECK(palette.colors[0] == 0x000000);   // Black
    CHECK(palette.colors[15] == 0xFFFFFF);  // White
}

// ═════════════════════════════════════════════════════════════════════════════
// Scaled Bitmap Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("ScaledBitmapV6 encode and decode") {
    SUBCASE("basic scaled bitmap") {
        ScaledBitmapV6 bitmap;
        bitmap.width = 640;
        bitmap.height = 480;
        bitmap.scale_x = 1.5f;
        bitmap.scale_y = 1.5f;
        bitmap.rotation_deg = 0.0f;
        bitmap.format = 8; // 24-bit RGB
        bitmap.options = 0x01;

        auto encoded = bitmap.encode();
        CHECK(encoded.size() >= 14);

        auto decoded = ScaledBitmapV6::decode(encoded);
        CHECK(decoded.is_ok());
        CHECK(decoded.value().width == 640);
        CHECK(decoded.value().height == 480);
        CHECK(decoded.value().scale_x == doctest::Approx(1.5f).epsilon(0.01));
        CHECK(decoded.value().scale_y == doctest::Approx(1.5f).epsilon(0.01));
        CHECK(decoded.value().format == 8);
    }

    SUBCASE("rotated bitmap") {
        ScaledBitmapV6 bitmap;
        bitmap.width = 800;
        bitmap.height = 600;
        bitmap.scale_x = 1.0f;
        bitmap.scale_y = 1.0f;
        bitmap.rotation_deg = 90.0f; // 90 degrees rotation

        auto encoded = bitmap.encode();
        auto decoded = ScaledBitmapV6::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().rotation_deg == doctest::Approx(90.0f).epsilon(0.1));
    }

    SUBCASE("non-uniform scaling") {
        ScaledBitmapV6 bitmap;
        bitmap.width = 400;
        bitmap.height = 300;
        bitmap.scale_x = 2.0f; // 2x horizontal
        bitmap.scale_y = 0.5f; // 0.5x vertical

        auto encoded = bitmap.encode();
        auto decoded = ScaledBitmapV6::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().scale_x == doctest::Approx(2.0f).epsilon(0.01));
        CHECK(decoded.value().scale_y == doctest::Approx(0.5f).epsilon(0.01));
    }

    SUBCASE("different pixel formats") {
        ScaledBitmapV6 bitmap;
        bitmap.format = 7; // 16-bit

        auto encoded = bitmap.encode();
        auto decoded = ScaledBitmapV6::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().format == 7);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Integration Tests
// ═════════════════════════════════════════════════════════════════════════════

TEST_CASE("VT6 complete object pool") {
    ObjectPool pool;

    SUBCASE("pool with VT6 objects") {
        // Create Window Mask with VT6 graphics
        VTObject window;
        window.id = 10000;
        window.type = ObjectType::WindowMask;
        pool.add_object(window);

        // Create Graphics Context
        VTObject gfx_ctx;
        gfx_ctx.id = 10001;
        gfx_ctx.type = ObjectType::GraphicsContext;
        GraphicsContextV6 ctx;
        ctx.fill_color_rgb = 0x4169E1; // RoyalBlue
        ctx.transparency = 200;
        gfx_ctx.body = ctx.encode();
        pool.add_object(gfx_ctx);

        // Create Scaled Bitmap
        VTObject scaled_bmp;
        scaled_bmp.id = 10002;
        scaled_bmp.type = ObjectType::ScaledBitmap;
        ScaledBitmapV6 bmp;
        bmp.width = 1024;
        bmp.height = 768;
        bmp.scale_x = 1.0f;
        bmp.scale_y = 1.0f;
        scaled_bmp.body = bmp.encode();
        pool.add_object(scaled_bmp);

        // Create External Object
        VTObject ext_obj;
        ext_obj.id = 10003;
        ext_obj.type = ObjectType::ExternalObjectDefinition;
        pool.add_object(ext_obj);

        CHECK(pool.object_count() == 4);
        CHECK(pool.has_vt6_objects());
    }
}

TEST_CASE("Touch gesture on VT6 object") {
    SUBCASE("tap gesture on button") {
        TouchGesture tap;
        tap.type = GestureType::Tap;
        tap.x = 100;
        tap.y = 50;
        tap.target_object = 5000; // Button object ID
        tap.duration_ms = 100;

        auto encoded = tap.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().target_object == 5000);
        // VT would process this tap and trigger button activation
    }

    SUBCASE("pinch gesture on graphics context") {
        TouchGesture pinch;
        pinch.type = GestureType::PinchOut;
        pinch.x = 320;
        pinch.y = 240;
        pinch.scale = 2.5f; // Zoom in 2.5x
        pinch.target_object = 10001; // Graphics context
        pinch.touch_count = 2;

        auto encoded = pinch.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().scale == doctest::Approx(2.5f).epsilon(0.01));
        // VT would apply zoom to graphics context
    }

    SUBCASE("rotate gesture on scaled bitmap") {
        TouchGesture rotate;
        rotate.type = GestureType::Rotate;
        rotate.x = 400;
        rotate.y = 300;
        rotate.rotation_deg = -30.0f; // 30 degrees counter-clockwise
        rotate.target_object = 10002; // Scaled bitmap
        rotate.touch_count = 2;

        auto encoded = rotate.encode();
        auto decoded = TouchGesture::decode(encoded);

        CHECK(decoded.is_ok());
        CHECK(decoded.value().rotation_deg == doctest::Approx(-30.0f).epsilon(0.1));
        // VT would rotate the bitmap
    }
}

TEST_CASE("VT6 graphics rendering pipeline") {
    SUBCASE("setup graphics context and draw") {
        // Create graphics context
        GraphicsContextV6 ctx;
        ctx.fill_color_rgb = 0x228B22;   // ForestGreen
        ctx.line_color_rgb = 0x000000;   // Black
        ctx.line_width = 3;
        ctx.transparency = 200;          // Slightly transparent
        ctx.anti_aliasing = true;
        ctx.blend_mode = 0;              // Normal blending

        // Create scaled bitmap to render
        ScaledBitmapV6 bitmap;
        bitmap.width = 800;
        bitmap.height = 600;
        bitmap.scale_x = 1.0f;
        bitmap.scale_y = 1.0f;
        bitmap.rotation_deg = 0.0f;
        bitmap.format = 8; // 24-bit RGB

        // Encode both
        auto ctx_data = ctx.encode();
        auto bmp_data = bitmap.encode();

        CHECK(ctx_data.size() > 0);
        CHECK(bmp_data.size() > 0);

        // VT rendering engine would:
        // 1. Apply graphics context settings
        // 2. Render bitmap with specified format
        // 3. Apply transparency and blending
        // 4. Draw to framebuffer
    }
}
