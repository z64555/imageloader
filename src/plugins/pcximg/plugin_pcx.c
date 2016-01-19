
#include "plugin_pcx.h"

#include <imageloader_plugin.h>

#include <inttypes.h>

// Parts of this code are based on this tutorial: http://www.piko3d.net/tutorials/libpcx-tutorial-loading-pcx-files-from-streams/

typedef struct
{
    pcx_structp pcx_ptr;
    pcx_infop info_ptr;
} PCXPointers;

#define pcx_error_occured(pcx_ptr) setjmp(pcx_jmpbuf(pcx_ptr)) != 0

static pcx_voidp pcx_malloc_fn(pcx_structp pcx_ptr, pcx_size_t size)
{
    ImgloadPlugin plugin = (ImgloadPlugin)pcx_get_mem_ptr(pcx_ptr);

    return imgload_plugin_realloc(plugin, NULL, size);
}

static void pcx_free_fn(pcx_structp pcx_ptr, pcx_voidp ptr)
{
    ImgloadPlugin plugin = (ImgloadPlugin)pcx_get_mem_ptr(pcx_ptr);

    imgload_plugin_free(plugin, ptr);
}


static void pcx_user_read_data(pcx_structp pcx_ptr, pcx_bytep data, pcx_size_t length)
{
    ImgloadImage img = (ImgloadImage)pcx_get_io_ptr(pcx_ptr);

    size_t read = imgload_plugin_image_read(img, (uint8_t*)data, length);

    if (read != length)
    {
        pcx_error(pcx_ptr, "Read Error");
    }
}


static void pcx_error_fn(pcx_structp pcx_ptr, pcx_const_charp message)
{
    ImgloadPlugin plugin = (ImgloadPlugin)pcx_get_error_ptr(pcx_ptr);

    imgload_plugin_log(plugin, IMGLOAD_LOG_ERROR, message);
    
    longjmp(pcx_jmpbuf(pcx_ptr), 1);
}

static void pcx_warning_fn(pcx_structp pcx_ptr, pcx_const_charp message)
{
    ImgloadPlugin plugin = (ImgloadPlugin)pcx_get_error_ptr(pcx_ptr);

    imgload_plugin_log(plugin, IMGLOAD_LOG_WARNING, message);
}


#define PCXSIGSIZE 8
static int IMGLOAD_CALLBACK pcx_probe(ImgloadPlugin plugin, ImgloadImage img)
{
    pcx_byte pcxsig[PCXSIGSIZE];

    if (imgload_plugin_image_read(img, pcxsig, PCXSIGSIZE) != PCXSIGSIZE)
    {
        return 0;
    }

    return pcx_sig_cmp(pcxsig, 0, PCXSIGSIZE) == 0;
}

static ImgloadErrorCode IMGLOAD_CALLBACK pcx_init_image(ImgloadPlugin plugin, ImgloadImage img)
{
    pcx_structp pcx_ptr = pcx_create_read_struct_2(PCX_LIBPCX_VER_STRING, plugin, pcx_error_fn, pcx_warning_fn, plugin, pcx_malloc_fn, pcx_free_fn);
    if (pcx_ptr == NULL)
    {
        return IMGLOAD_ERR_PLUGIN_ERROR;
    }

    pcx_infop pcx_info = pcx_create_info_struct(pcx_ptr);
    if (pcx_info == NULL)
    {
        pcx_destroy_read_struct(&pcx_ptr, NULL, NULL);
        return IMGLOAD_ERR_PLUGIN_ERROR;
    }

    if (pcx_error_occured(pcx_ptr))
    {
        // libPCX has caused an error, free memory and return
        pcx_destroy_read_struct(&pcx_ptr, &pcx_info, NULL);
        return IMGLOAD_ERR_PLUGIN_ERROR;
    }

    pcx_set_read_fn(pcx_ptr, img, pcx_user_read_data);

    pcx_set_sig_bytes(pcx_ptr, PCXSIGSIZE);

    pcx_read_info(pcx_ptr, pcx_info);

    // Info has been read
    pcx_uint_32 img_width = pcx_get_image_width(pcx_ptr, pcx_info);
    pcx_uint_32 img_height = pcx_get_image_height(pcx_ptr, pcx_info);

    pcx_uint_32 bitdepth = pcx_get_bit_depth(pcx_ptr, pcx_info);
    pcx_uint_32 color_type = pcx_get_color_type(pcx_ptr, pcx_info);

    if (bitdepth == 16)
    {
        pcx_set_strip_16(pcx_ptr);
    }

    switch (color_type) {
    case PCX_COLOR_TYPE_PALETTE:
        // Expand palette to rgb
        pcx_set_palette_to_rgb(pcx_ptr);
        break;
    case PCX_COLOR_TYPE_GRAY:
        if (bitdepth < 8)
            pcx_set_expand_gray_1_2_4_to_8(pcx_ptr);
        break;
    }

    // if the image has a transperancy set.. convert it to a full Alpha channel..
    // Also make sure that is was RGB before
    if (pcx_get_valid(pcx_ptr, pcx_info, PCX_INFO_tRNS))
    {
        pcx_set_tRNS_to_alpha(pcx_ptr);
    }

    // Update the structure so we can use it later
    pcx_read_update_info(pcx_ptr, pcx_info);

    bitdepth = pcx_get_bit_depth(pcx_ptr, pcx_info);
    uint32_t channels = pcx_get_channels(pcx_ptr, pcx_info);
    color_type = pcx_get_color_type(pcx_ptr, pcx_info);

    if (bitdepth != 8)
    {
        // Any bitdepth != 8 is not supported
        pcx_destroy_read_struct(&pcx_ptr, &pcx_info, NULL);

        imgload_plugin_log(plugin, IMGLOAD_LOG_ERROR, "PCX has a bitdepth of %"PRIu32" but only a bitdepth of 8 is supported by this plugin!", bitdepth);

        return IMGLOAD_ERR_UNSUPPORTED_FORMAT;
    }

    // Convert PCX format to imageloader. Also validates channel number
    ImgloadFormat format;
    if (color_type == PCX_COLOR_TYPE_RGB && channels == 3)
    {
        format = IMGLOAD_FORMAT_R8G8B8;
    } else if (color_type == PCX_COLOR_TYPE_RGBA && channels == 4)
    {
        format = IMGLOAD_FORMAT_R8G8B8A8;
    } else if (color_type == PCX_COLOR_TYPE_GRAY && channels == 1)
    {
        format = IMGLOAD_FORMAT_GRAY8;
    } else
    {
        // Currently no other format is supported
        pcx_destroy_read_struct(&pcx_ptr, &pcx_info, NULL);

        imgload_plugin_log(plugin, IMGLOAD_LOG_ERROR, "PCX has an unsupported data format!");

        return IMGLOAD_ERR_UNSUPPORTED_FORMAT;
    }

    // Everything seems to be alright, set imageloader properties
    imgload_plugin_image_set_data_type(img, format, IMGLOAD_COMPRESSION_NONE);
    imgload_plugin_image_set_num_frames(img, 1);
    imgload_plugin_image_set_num_mipmaps(img, 0, 1);

    imgload_plugin_image_set_property(img, 0, IMGLOAD_PROPERTY_WIDTH, IMGLOAD_PROPERTY_TYPE_UINT32, &img_width);
    imgload_plugin_image_set_property(img, 0, IMGLOAD_PROPERTY_HEIGHT, IMGLOAD_PROPERTY_TYPE_UINT32, &img_height);

    // PCXs are always 2D so set depth to 1
    uint32_t one = 1;
    imgload_plugin_image_set_property(img, 0, IMGLOAD_PROPERTY_DEPTH, IMGLOAD_PROPERTY_TYPE_UINT32, &one);

    PCXPointers* pointers = (PCXPointers*)imgload_plugin_realloc(plugin, NULL, sizeof(PCXPointers));
    if (pointers == NULL)
    {
        // Currently no other format is supported
        pcx_destroy_read_struct(&pcx_ptr, &pcx_info, NULL);
        return IMGLOAD_ERR_OUT_OF_MEMORY;
    }

    pointers->pcx_ptr = pcx_ptr;
    pointers->info_ptr = pcx_info;

    imgload_plugin_image_set_data(img, pointers);
    return IMGLOAD_ERR_NO_ERROR;
}

static ImgloadErrorCode IMGLOAD_CALLBACK pcx_read_data(ImgloadPlugin plugin, ImgloadImage img)
{
    PCXPointers* pointers = (PCXPointers*)imgload_plugin_image_get_data(img);

    pcx_structp pcx_ptr = pointers->pcx_ptr;
    pcx_infop pcx_info = pointers->info_ptr;

    pcx_uint_32 img_width = pcx_get_image_width(pcx_ptr, pcx_info);
    pcx_uint_32 img_height = pcx_get_image_height(pcx_ptr, pcx_info);

    //Here's one of the pointers we've defined in the error handler section:
    //Array of row pointers. One for every row.
    pcx_bytepp rowPtrs = (pcx_bytepp)imgload_plugin_realloc(plugin, NULL, img_height * sizeof(pcx_bytep));
    if (rowPtrs == NULL)
    {
        return IMGLOAD_ERR_OUT_OF_MEMORY;
    }

    //This is the length in bytes, of one row.
    size_t stride = pcx_get_rowbytes(pcx_ptr, pcx_info);
    size_t total_size = img_height * stride;

    //Allocate a buffer with enough space.
    pcx_byte* data = (pcx_byte*)imgload_plugin_realloc(plugin, NULL, total_size);
    if (data == NULL)
    {
        imgload_plugin_free(plugin, rowPtrs);
        return IMGLOAD_ERR_OUT_OF_MEMORY;
    }

    //A little for-loop here to set all the row pointers to the starting
    //Adresses for every row in the buffer

    for (size_t i = 0; i < img_height; i++) {
        size_t q = i * stride;
        rowPtrs[i] = data + q;
    }

    if (pcx_error_occured(pcx_ptr))
    {
        // Something went wrong, PANIC!!!
        imgload_plugin_free(plugin, rowPtrs);
        imgload_plugin_free(plugin, data);

        return IMGLOAD_ERR_PLUGIN_ERROR;
    }

    //And here it is! The actuall reading of the image!
    //Read the imagedata and write it to the adresses pointed to
    //by rowptrs (in other words: our image databuffer)
    pcx_read_image(pcx_ptr, rowPtrs);

    // Everything should be fine here, now set the data and go home
    ImgloadImageData img_data;
    img_data.width = img_width;
    img_data.height = img_height;
    img_data.depth = 1;

    img_data.stride = stride;
    img_data.data_size = total_size;
    img_data.data = data;

    // The memory was allocated using the imageloader allocator so we can transfer ownership
    ImgloadErrorCode err = imgload_plugin_image_set_image_data(img, 0, 0, &img_data, 1);

    // The row pointers aren't needed anymore
    imgload_plugin_free(plugin, rowPtrs);

    return err;
}

static ImgloadErrorCode IMGLOAD_CALLBACK pcx_deinit_image(ImgloadPlugin plugin, ImgloadImage img)
{
    PCXPointers* pointers = (PCXPointers*)imgload_plugin_image_get_data(img);

    pcx_destroy_read_struct(&pointers->pcx_ptr, &pointers->info_ptr, NULL);
    imgload_plugin_free(plugin, pointers);

    return IMGLOAD_ERR_NO_ERROR;
}

ImgloadErrorCode IMGLOAD_CALLBACK pcx_plugin_loader(ImgloadPlugin plugin, void* parameter)
{
    imgload_plugin_set_info(plugin, "pcx", "libPCX plugin", "Loads PCX files using libpcx");

    imgload_plugin_callback_probe(plugin, pcx_probe);

    imgload_plugin_callback_init_image(plugin, pcx_init_image);
    imgload_plugin_callback_deinit_image(plugin, pcx_deinit_image);

    imgload_plugin_callback_read_data(plugin, pcx_read_data);

    return IMGLOAD_ERR_NO_ERROR;
}