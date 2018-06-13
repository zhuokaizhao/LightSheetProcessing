//
// Created by Robin Weiss
//

#ifndef LSP_SKIMCZI_UTIL_H
#define LSP_SKIMCZI_UTIL_H

#include <libxml/tree.h>
#include "skimczi.h"

typedef struct{
    int sizeX;
    int sizeY;
    int sizeZ;
    int sizeC;
    int sizeT;
    double scalingX;
    double scalingY;
    double scalingZ;
    CziPixelType pixelType;
    size_t pixelSize;
} ImageDims;

CziPixelType ConvertStringToPixelType(const char *wszValue);

void get_image_dims(xmlNode *a_node, ImageDims *dims);

#endif //LSP_SKIMCZI_UTIL_H
