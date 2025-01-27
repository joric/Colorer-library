#include "colorer/handlers/StyledHRDMapper.h"
#include <xercesc/dom/DOM.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include "colorer/Exception.h"
#include "colorer/base/XmlTagDefs.h"
#include "colorer/common/UStr.h"
#include "colorer/xml/XmlParserErrorHandler.h"

StyledHRDMapper::~StyledHRDMapper()
{
  regionDefines.clear();
}

void StyledHRDMapper::loadRegionMappings(XmlInputSource& is)
{
  xercesc::XercesDOMParser xml_parser;
  XmlParserErrorHandler error_handler;

  xml_parser.setErrorHandler(&error_handler);
  xml_parser.setLoadExternalDTD(false);
  xml_parser.setLoadSchema(false);
  xml_parser.setSkipDTDValidation(true);
  xml_parser.parse(*is.getInputSource());

  if (error_handler.getSawErrors()) {
    throw Exception("Error loading HRD file '" + *is.getPath() + "'");
  }

  xercesc::DOMDocument* hrdbase = xml_parser.getDocument();
  xercesc::DOMElement* hbase = hrdbase->getDocumentElement();

  if (!hbase || !xercesc::XMLString::equals(hbase->getNodeName(), hrdTagHrd)) {
    throw Exception("Incorrect hrd-file structure. Main '<hrd>' block not found. Current file " +
                    *is.getPath());
  }

  for (xercesc::DOMNode* curel = hbase->getFirstChild(); curel; curel = curel->getNextSibling()) {
    if (curel->getNodeType() == xercesc::DOMNode::ELEMENT_NODE &&
        xercesc::XMLString::equals(curel->getNodeName(), hrdTagAssign))
    {
      if (auto* subelem = dynamic_cast<xercesc::DOMElement*>(curel)) {
        const XMLCh* xname = subelem->getAttribute(hrdAssignAttrName);
        if (UStr::isEmpty(xname)) {
          continue;
        }

        UnicodeString name(xname);
        auto rd_new = regionDefines.find(name);
        if (rd_new != regionDefines.end()) {
          spdlog::warn("Duplicate region name '{0}' in file '{1}'. Previous value replaced.", name,
                       *is.getPath());
          regionDefines.erase(rd_new);
        }

        unsigned int fore = 0;
        bool bfore = false;
        const XMLCh* sval = subelem->getAttribute(hrdAssignAttrFore);
        if (!UStr::isEmpty(sval)) {
          bfore = UStr::HexToUInt(UnicodeString(sval), &fore);
        }

        unsigned int back = 0;
        bool bback = false;
        sval = subelem->getAttribute(hrdAssignAttrBack);
        if (!UStr::isEmpty(sval)) {
          bback = UStr::HexToUInt(UnicodeString(sval), &back);
        }

        unsigned int style = 0;
        sval = subelem->getAttribute(hrdAssignAttrStyle);
        if (!UStr::isEmpty(sval)) {
          UStr::HexToUInt(UnicodeString(sval), &style);
        }

        auto rdef = std::make_unique<StyledRegion>(bfore, bback, fore, back, style);
        regionDefines.emplace(name, std::move(rdef));
      }
    }
  }
}

/** Writes all currently loaded region definitions into
    XML file. Note, that this method writes all loaded
    defines from all loaded HRD files.
*/
void StyledHRDMapper::saveRegionMappings(Writer* writer) const
{
  writer->write("<?xml version=\"1.0\"?>\n");
  for (const auto& regionDefine : regionDefines) {
    const StyledRegion* rdef = StyledRegion::cast(regionDefine.second.get());
    char temporary[256];
    constexpr auto size_temporary = std::size(temporary);
    writer->write("  <define name='" + regionDefine.first + "'");
    if (rdef->isForeSet) {
      snprintf(temporary, size_temporary, " fore=\"#%06x\"", rdef->fore);
      writer->write(temporary);
    }
    if (rdef->isBackSet) {
      snprintf(temporary, size_temporary, " back=\"#%06x\"", rdef->back);
      writer->write(temporary);
    }
    if (rdef->style) {
      snprintf(temporary, size_temporary, " style=\"%u\"", rdef->style);
      writer->write(temporary);
    }
    writer->write("/>\n");
  }
  writer->write("\n</hrd>\n");
}

/** Adds or replaces region definition */
void StyledHRDMapper::setRegionDefine(const UnicodeString& name, const RegionDefine* rd)
{
  if (!rd)
    return;

  const StyledRegion* new_region = StyledRegion::cast(rd);
  RegionDefine* rd_new = new StyledRegion(*new_region);

  auto rd_old_it = regionDefines.find(name);
  if (rd_old_it == regionDefines.end()) {
    std::pair<UnicodeString, RegionDefine*> pp(name, rd_new);
    regionDefines.emplace(pp);
  }
  else {
    rd_old_it->second.reset(rd_new);
  }
}
