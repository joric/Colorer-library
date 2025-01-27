#include "colorer/xml/BaseEntityResolver.h"
#include "colorer/xml/XmlInputSource.h"
#include <xercesc/util/XMLString.hpp>

xercesc::InputSource* BaseEntityResolver::resolveEntity(xercesc::XMLResourceIdentifier* resourceIdentifier)
{
  if (xercesc::XMLString::startsWith(resourceIdentifier->getBaseURI(), kJar) ||
      xercesc::XMLString::startsWith(resourceIdentifier->getSystemId(), kJar) ||
      xercesc::XMLString::findAny(resourceIdentifier->getSystemId(), kPercent)) {
    auto input_source = XmlInputSource::newInstance(resourceIdentifier->getSystemId(), resourceIdentifier->getBaseURI());
    return input_source.release()->getInputSource();
  }
  return nullptr;
}
