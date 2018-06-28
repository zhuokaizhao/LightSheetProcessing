//! \file skimczi_util.h
//! \author Robin Weiss

#ifndef LSP_SKIMCZI_UTIL_H
#define LSP_SKIMCZI_UTIL_H

#include <libxml/tree.h>
#include "skimczi.h"

CziPixelType ConvertStringToPixelType(const char *wszValue);

//! \brief Read image info from CZI file.
void get_image_dims(xmlNode *a_node, ImageDims *dims);

#endif //LSP_SKIMCZI_UTIL_H
