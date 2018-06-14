//
// Created by Robin Weiss
//

#include "skimczi.h"
#include "skimczi_util.h"

#include <cfloat>
#include <string.h>


// Convert the string parameter to a CZI pixel type.
CziPixelType ConvertStringToPixelType(const char *wszValue) {

  CziPixelType pixeltype = CZIPIXELTYPE_UNDEFINED;
  if(wszValue) {
    if(!strcmp("Gray8", wszValue))
      pixeltype = CZIPIXELTYPE_GRAY8;
    else if(!strcmp("Gray16", wszValue))
      pixeltype = CZIPIXELTYPE_GRAY16;
    else if(!strcmp("Gray32Float", wszValue))
      pixeltype = CZIPIXELTYPE_GRAY32FLOAT;
    else if(!strcmp("Bgr24", wszValue))
      pixeltype = CZIPIXELTYPE_BGR24;
    else if(!strcmp("Bgr48", wszValue))
      pixeltype = CZIPIXELTYPE_BGR48;
    else if(!strcmp("Bgr96Float", wszValue))
      pixeltype = CZIPIXELTYPE_BGR96FLOAT;
    else if(!strcmp("Bgra32", wszValue))
      pixeltype = CZIPIXELTYPE_BGRA32;
    else if(!strcmp("Gray64ComplexFloat", wszValue))
      pixeltype = CZIPIXELTYPE_GRAY64COMPEXFLOAT;
    else if(!strcmp("Bgr192ComplexFloat", wszValue))
      pixeltype = CZIPIXELTYPE_BGR192COMPEXFLOAT;
  }
  return pixeltype;
}

void get_image_dims(xmlNode * a_node, ImageDims * dims) {
  xmlNode *cur_node = NULL;
  xmlChar *key;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SizeX")){
        key = xmlNodeGetContent(cur_node);
        dims->sizeX = atoi((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SizeY")){
        key = xmlNodeGetContent(cur_node);
        dims->sizeY = atoi((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SizeZ")){
        key = xmlNodeGetContent(cur_node);
        dims->sizeZ = atoi((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SizeC")){
        key = xmlNodeGetContent(cur_node);
        dims->sizeC = atoi((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"SizeT")){
        key = xmlNodeGetContent(cur_node);
        dims->sizeT = atoi((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"ScalingX")){
        key = xmlNodeGetContent(cur_node);
        dims->scalingX = atof((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"ScalingY")){
        key = xmlNodeGetContent(cur_node);
        dims->scalingY = atof((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"ScalingZ")){
        key = xmlNodeGetContent(cur_node);
        dims->scalingZ = atof((const char *)key);
        xmlFree(key);
      }
      if (!xmlStrcmp(cur_node->name, (const xmlChar *)"PixelType")){
        key = xmlNodeGetContent(cur_node);
        dims->pixelType = ConvertStringToPixelType((const char *)key);
        if (dims->pixelType == CZIPIXELTYPE_GRAY8){
          dims->pixelSize = sizeof(char);
        }
        else if (dims->pixelType == CZIPIXELTYPE_GRAY16){
          dims->pixelSize = sizeof(short);
        }
        else if (dims->pixelType == CZIPIXELTYPE_GRAY32FLOAT){
          dims->pixelSize = sizeof(float);
        }
        xmlFree(key);
      }
    }
    get_image_dims(cur_node->children, dims);
  }
}
