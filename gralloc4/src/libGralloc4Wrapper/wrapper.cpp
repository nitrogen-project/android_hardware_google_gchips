#include "gralloc4/gralloc_vendor_interface.h"
#include <vector>
#include <sys/stat.h>

#include "core/format_info.h"
#include "core/mali_gralloc_bufferdescriptor.h"
#include "core/mali_gralloc_bufferallocation.h"
#include "allocator/mali_gralloc_ion.h"

namespace android::hardware::graphics::allocator::priv {

struct Descriptor {
    unsigned int size = 0;
    uint64_t producer_usage = 0;
    uint64_t consumer_usage = 0;

    struct PlaneDescriptor {
        int fd = -1;
        size_t size = 0;
        off_t offset = 0;
        int stride_byte = 0;
    };
    std::vector<PlaneDescriptor> planes;

    int width = 0;
    int height = 0;
    int stride_pixel = 0;
    int format = 0;
};

Descriptor *createDescriptor() { return new Descriptor(); }
void deleteDescriptor(Descriptor *descriptor) { delete descriptor; }

void setProducerUsage(Descriptor &descriptor, uint64_t usage) {
    descriptor.producer_usage = usage;
}

void setConsumerUsage(Descriptor &descriptor, uint64_t usage) {
    descriptor.consumer_usage = usage;
}

void setPlaneCount(Descriptor &descriptor, int count) {
    descriptor.planes.resize(count);
}

void setPlane(Descriptor &descriptor, int index, int fd, size_t size, off_t offset, int stride_byte) {
    descriptor.planes[index].fd = fd;
    descriptor.planes[index].size = size;
    descriptor.planes[index].offset = offset;
    descriptor.planes[index].stride_byte = stride_byte;
}

void setWidth(Descriptor &descriptor, int width) {
    descriptor.width = width;
}

void setHeight(Descriptor &descriptor, int height) {
    descriptor.height = height;
}

void setStridePixel(Descriptor &descriptor, int stride_pixel) {
    descriptor.stride_pixel = stride_pixel;
}

void setFormat(Descriptor &descriptor, int format) {
    descriptor.format = format;
}

buffer_handle_t createNativeHandle(const Descriptor &descriptor) {
    for (int i = 0; i < descriptor.planes.size(); ++i) {
        struct stat st;
        fstat(descriptor.planes[i].fd, &st);
        off64_t fd_size = st.st_size;
        if (fd_size < descriptor.planes[i].size) {
            ALOGE("libGralloc4Wrapper: createNativeHandle failed: plane[%d] requested size greater than fd size.",
                i);
            return nullptr;
        }
    }

    buffer_descriptor_t buffer_descriptor;

    buffer_descriptor.pixel_stride = descriptor.stride_pixel;
    buffer_descriptor.width = descriptor.width;
    buffer_descriptor.height = descriptor.height;
    buffer_descriptor.layer_count = 1;
    buffer_descriptor.hal_format = buffer_descriptor.alloc_format
        = descriptor.format;
    buffer_descriptor.producer_usage = descriptor.producer_usage;
    buffer_descriptor.consumer_usage = descriptor.consumer_usage;
    buffer_descriptor.format_type = MALI_GRALLOC_FORMAT_TYPE_USAGE;
    buffer_descriptor.signature = sizeof(buffer_descriptor_t);

    buffer_descriptor.fd_count = buffer_descriptor.plane_count
        = descriptor.planes.size();
    for (int i = 0; i < descriptor.planes.size(); ++i) {
        buffer_descriptor.alloc_sizes[i] = descriptor.planes[i].size;
    }

    auto format_index = get_format_index(descriptor.format);
    if (format_index == -1) {
        ALOGE("libGralloc4Wrapper: invalid format 0x%x",
            descriptor.format);
        return 0;
    }
    for (int i = 0; i < descriptor.planes.size(); ++i) {
        uint8_t bpp = formats[format_index].bpp[i];
        if (bpp == 0) {
            ALOGE("libGralloc4Wrapper: format 0x%x has bpp[%d]=0",
                descriptor.format, i);
            return nullptr;
        }
        buffer_descriptor.plane_info[i] = {
            .byte_stride = static_cast<uint32_t>((descriptor.planes[i].stride_byte * bpp) / 8),
            .alloc_width = buffer_descriptor.width,
            .alloc_height = buffer_descriptor.height,
        };
    }

    if (mali_gralloc_derive_format_and_size(&buffer_descriptor)) {
        ALOGE("libGralloc4Wrapper: mali_gralloc_derive_format_and_size failed");
        return nullptr;
    }

    const gralloc_buffer_descriptor_t gralloc_buffer_descriptor =
        reinterpret_cast<const gralloc_buffer_descriptor_t>(&buffer_descriptor);

    buffer_handle_t tmp_buffer;
    bool shared;
    // TODO(modan@, make mali_gralloc_ion_allocate accept multiple fds)
    int allocResult = mali_gralloc_ion_allocate(&gralloc_buffer_descriptor, 1, &tmp_buffer, &shared);
    if (allocResult < 0) {
        ALOGE("mali_gralloc_ion_allocate failed");
        return nullptr;
    }

    private_handle_t *private_handle = const_cast<private_handle_t *>(
        static_cast<const private_handle_t *>(tmp_buffer));
    // TODO(modan@, handle all plane offsets)
    private_handle->offset = private_handle->plane_info[0].offset = descriptor.planes[0].offset;

    return tmp_buffer;
}
}  // namespace android::hardware::graphics::allocator::priv
