#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdint.h>

// clang-format off


enum { GRPC_MAX_MESSAGE_SIZE = 16 * 1024 * 1024 };


// mappable data

#define FOI_EVENT_X(macro) \
    macro(std::string, fov_id) \
    macro(uint64_t, sdu_id) \
    macro(uint64_t, timestamp) \
    macro(std::string, coordinate)

#define FOI_OBJECT_X(macro) \
    macro(int32_t, x) \
    macro(int32_t, y) \
    macro(int32_t, w) \
    macro(int32_t, h) \
    macro(float, metric) \
    macro(float, centroid_x) \
    macro(float, centroid_y) \
    macro(std::string, label) \
    macro(float, score)

#define FOI_IMAGE_X(macro) macro(int32_t, w) macro(int32_t, h)

#define FOI_NOTIFY_X(macro) \
    macro(std::string, fov_id) \
    macro(uint64_t, timestamp) \
    macro(int32_t, frame_x) \
    macro(int32_t, frame_y) \
    macro(int32_t, frame_width) \
    macro(int32_t, frame_height) \
    macro(float, metric) \
    macro(uint32_t, object_width) \
    macro(uint32_t, object_height) \
    macro(std::string, category) \
    macro(uint64_t, sdu_id) \
    macro(std::string, coordinate) \
    macro(uint32_t, status)


#define DECL_MACRO(type, name) type name;

// clang-format on

/*!
 * \brief The PlainFoiObject struct
 */
struct PlainFoiObject
{
    FOI_OBJECT_X(DECL_MACRO)
};

/*!
 * \brief The PlainFoiImage struct
 */
struct PlainFoiImage
{
    FOI_IMAGE_X(DECL_MACRO)

    std::vector<char> data;
};

/*!
 * \brief The PlainFoiEvent struct
 */
struct PlainFoiEvent
{
    FOI_EVENT_X(DECL_MACRO)

    std::shared_ptr<const PlainFoiImage> image;
    std::vector<PlainFoiObject> objects;
};

/*!
 * \brief The PlainFoiNotify struct
 */
struct PlainFoiNotify
{
    FOI_NOTIFY_X(DECL_MACRO)

    std::vector<std::shared_ptr<const PlainFoiImage>> images;
};

#undef DECL_MACRO
