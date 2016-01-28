#ifndef PCX_H
#define PCX_H
#pragma once
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Name:        pcximg/pcx.h
// Purpose:     PCX Data type
// Author:      z64555          ( z1281110@gmail.com )
// Created:     18/01/2016      (DD/MM/YYYY)
// Page width:  120 char
// Tab width:   4 spaces
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PCX_EGA_PALETTE_SIZE     48U    // Number of bytes needed for an EGA Palette (16 colors)
#define PCX_VGA_PALETTE_SIZE    768U    // Number of bytes in a VGA Palette (256 colors)
#define PCX_RESERVED2_SIZE       54U    // Number of bytes in the Reserved2 array

enum
{
	PCX_ERR_NO_ERROR = 0,       //!< No error found
	PCX_ERR_NO_DATA,            //!< Error: No data loaded. Did you init?
};
typedef uint32_t PcxErrorCode;

typedef struct
{
	const uint8_t Identifier;
	uint8_t  Version;
	uint8_t  Encoding;
	uint8_t  BitsPerPixel;
	uint16_t XStart;
	uint16_t YStart;
	uint16_t XEnd;
	uint16_t YEnd;
	uint16_t HorzRes;
	uint16_t VertRes;
	uint8_t  Palette[PCX_EGA_PALETTE_SIZE]; //!< EGA color palette
	uint8_t  Reserved1;                     //!< Unused
	uint8_t  NumBitPlanes;
	uint16_t BytesPerLine;
	uint16_t PaletteType;
	uint16_t HorzScreenSize;
	uint16_t VertScreenSize;
	uint8_t  Reserved2[PCX_RESERVED2_SIZE]; //!< Unused

	uint8_t* ImageData;     //!< Pointer to the image data, 

	uint8_t* PaletteVGA;    //!< The VGA palette (optional)
} PCX;

typedef struct
{
	char*    Name;              //!< Name of the image
	uint32_t Buffer_size;       //!< Size, in bytes, of the heap buffer containing the ImageData (and possibly a palette too)
} PCX_info;


/**
 * @brief init's a given PCX
 */
void pcx_init(PCX* img, PCX_info* info);

/**
 * @brief de-init's a given PCX and frees any allocated memory
 */
void pcx_deinit(PCX* img, PCX_info* info);

/**
 * @brief loads a PCX image from file into the PCX and PCX_info structures
 */
void pcx_load(PCX* img, PCX_info* info, const char* filename);

/**
 * @returns 0 If no VGA palette present
 * @returns 1 If VGA palette present
 */
int pcx_has_vga(PCX* img, PCX_info* info);


#ifdef __cplusplus
}
#endif

#endif // PAK_PCX_H

