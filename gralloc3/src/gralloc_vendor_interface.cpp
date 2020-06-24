#define GRALLOC_LIBRARY_BUILD

#include <gralloc3/gralloc_vendor_interface.h>

#include <android/hardware/graphics/mapper/2.0/IMapper.h>

#include "GrallocBufferDescriptor.h"
#include "mali_gralloc_buffer.h"
#include "mali_gralloc_bufferallocation.h"
#include "mali_gralloc_ion.h"
#include "mali_gralloc_formats.h"
#include "gralloc_buffer_priv.h"
#include "exynos_format.h"
#include "format_info.h"

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace priv {

struct Descriptor {
    unsigned int size;
    int fd;
    uint64_t producer_usage;
    uint64_t consumer_usage;
    off_t offset;
    int stride;
    int width;
    int height;
    int format;
};

Descriptor *createDescriptor() { return new Descriptor(); }
void deleteDescriptor(Descriptor *descriptor) { delete descriptor; }

void setFd(Descriptor *descriptor, int fd) {
    descriptor->fd = fd;
}

void setSize(Descriptor *descriptor, int size) {
    descriptor->size = size;
}

void setProducerUsage(Descriptor *descriptor, uint64_t usage) {
    descriptor->producer_usage = usage;
}

void setConsumerUsage(Descriptor *descriptor, uint64_t usage) {
    descriptor->consumer_usage = usage;
}

void setOffset(Descriptor *descriptor, off_t offset) {
    descriptor->offset = offset;
}

void setStride(Descriptor *descriptor, int stride) {
    descriptor->stride = stride;
}

void setWidth(Descriptor *descriptor, int width) {
    descriptor->width = width;
}

void setHeight(Descriptor *descriptor, int height) {
    descriptor->height = height;
}

void setFormat(Descriptor *descriptor, int format) {
    descriptor->format = format;
}

uint8_t getFormatBitsPerPixel(uint64_t format) {
    auto idx = get_format_index(format);
    if (idx == -1) {
        ALOGE("getFormatBitsPerPixel failed, invalid format.");
        return 0;
    }
    return formats[idx].bpp[0];
}

buffer_handle_t createNativeHandle(Descriptor *descriptor) {
    off64_t fd_size = lseek64(descriptor->fd, 0, SEEK_END);
    if (fd_size < descriptor->size) {
        ALOGE("createNativeHandle failed: requested size greater than fd size.");
        return nullptr;
    }

    buffer_descriptor_t buffer_descriptor;

    buffer_descriptor.pixel_stride = descriptor->stride;
    buffer_descriptor.width = descriptor->width;
    buffer_descriptor.height = descriptor->height;
    buffer_descriptor.layer_count = 1;
    buffer_descriptor.hal_format = descriptor->format;
    buffer_descriptor.producer_usage = descriptor->producer_usage;
    buffer_descriptor.consumer_usage = descriptor->consumer_usage;
    buffer_descriptor.format_type = MALI_GRALLOC_FORMAT_TYPE_USAGE;
    buffer_descriptor.signature = sizeof(buffer_descriptor_t);

    buffer_descriptor.alloc_format = buffer_descriptor.internal_format
        = buffer_descriptor.hal_format;
    buffer_descriptor.fd_count = 1;
    buffer_descriptor.plane_count = 1;
    buffer_descriptor.size = descriptor->size;
    uint8_t bpp = getFormatBitsPerPixel(descriptor->format);
    if (bpp == 0) {
        ALOGE("createNativeHandle failed, bpp == 0.");
        return nullptr;
    }
    buffer_descriptor.plane_info[0] = {
        .byte_stride = static_cast<uint32_t>(descriptor->stride / (bpp / 8)),
        .alloc_width = buffer_descriptor.width,
        .alloc_height = buffer_descriptor.height,
    };

    const gralloc_buffer_descriptor_t gralloc_buffer_descriptor =
        reinterpret_cast<const gralloc_buffer_descriptor_t>(&buffer_descriptor);

    buffer_handle_t tmp_buffer;
    bool shared;
    int allocResult = mali_gralloc_ion_allocate(&gralloc_buffer_descriptor, 1, &tmp_buffer, &shared,
        descriptor->fd);
    if (allocResult < 0) {
        return nullptr;
    }
    private_handle_t *private_handle = const_cast<private_handle_t *>(
        static_cast<const private_handle_t *>(tmp_buffer));
    int err = gralloc_buffer_attr_allocate(private_handle);
    if (err) {
        ALOGE("createNativeHandle failed, gralloc_buffer_attr_allocate returned %d.", err);
        gralloc_buffer_attr_free(private_handle);
        mali_gralloc_ion_free(private_handle);
        return nullptr;
    }
    return tmp_buffer;
}

int getFd(native_handle_t *handle) {
    return static_cast<private_handle_t *>(handle)->share_fd;
}

}  // namespace priv
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android
