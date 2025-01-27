#include "colorer/xml/XmlInputSource.h"
#include "colorer/Exception.h"
#include "colorer/common/UStr.h"
#include "colorer/xml/LocalFileXmlInputSource.h"
#if COLORER_FEATURE_JARINPUTSOURCE
#include "colorer/xml/ZipXmlInputSource.h"
#endif
#include <filesystem>
#include "colorer/utils/Environment.h"

uXmlInputSource XmlInputSource::newInstance(const UnicodeString* path, const UnicodeString* base)
{
  return newInstance(UStr::to_xmlch(path).get(), UStr::to_xmlch(base).get());
}

uXmlInputSource XmlInputSource::newInstance(const XMLCh* path, const XMLCh* base)
{
  if (!path || (*path == '\0')) {
    throw InputSourceException("XmlInputSource::newInstance: path is empty");
  }
  if (xercesc::XMLString::startsWith(path, kJar) ||
      (base != nullptr && xercesc::XMLString::startsWith(base, kJar)))
  {
#if COLORER_FEATURE_JARINPUTSOURCE
    return std::make_unique<ZipXmlInputSource>(path, base);
#else
    throw InputSourceException("ZipXmlInputSource not supported");
#endif
  }
  return std::make_unique<LocalFileXmlInputSource>(path, base);
}

std::filesystem::path XmlInputSource::getClearFilePath(const UnicodeString* basePath,
                                                       const UnicodeString* relPath)
{
  std::filesystem::path fs_basepath;
  if (basePath && !basePath->isEmpty()) {
    auto clear_basepath = Environment::normalizeFsPath(basePath);
    fs_basepath = std::filesystem::path(clear_basepath).parent_path();
  }
  auto clear_relpath = Environment::normalizeFsPath(relPath);

  std::filesystem::path full_path;
  if (fs_basepath.empty()) {
    full_path = clear_relpath;
  }
  else {
    full_path = fs_basepath / clear_relpath;
  }

  full_path = full_path.lexically_normal();

  return full_path;
}

bool XmlInputSource::isUriFile(const UnicodeString& path, const UnicodeString* base)
{
  if ((path.startsWith(kJar)) || (base && base->startsWith(kJar))) {
    return false;
  }
  return true;
}

uXmlInputSource XmlInputSource::createRelative(const XMLCh* relPath)
{
  return newInstance(relPath, this->getInputSource()->getSystemId());
}

UnicodeString* XmlInputSource::getPath() const
{
  return source_path.get();
}
