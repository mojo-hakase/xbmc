#pragma once

#include <list>
#include <map>

#include "DVDInputStreams/DVDInputStreamFile.h"
#include "MatroskaParser.h"

using OpenedSegmentMap = std::map<MatroskaSegmentUID,std::unique_ptr<CDVDInputStreamFile>>;
class MatroskaSegmentCache
{
  struct CacheEntry;
  using SharedCacheEntry = std::shared_ptr<CacheEntry>;
  using CacheList = std::list<SharedCacheEntry>;
  using SegmentCache = std::map<MatroskaSegmentUID,SharedCacheEntry>;
  using FileCache = std::multimap<std::string,SharedCacheEntry>;
  struct SegmentLocator
  {
    std::string filename;
    int64_t offset;
  };
  struct CacheEntry
  {
    SegmentLocator segmentLocator;
    CacheList::iterator cacheListIterator;
    SegmentCache::iterator segmentCacheIterator;
    FileCache::iterator fileCacheIterator;
  };
  
  CacheList cacheList;
  SegmentCache segmentCache;
  FileCache fileCache;

  CacheList::iterator scannedFileIterator;

  SharedCacheEntry LookUp(const MatroskaSegmentUID &uid);
  std::unique_ptr<CDVDInputStreamFile> OpenSegment(const SegmentLocator &locator);
  std::unique_ptr<CDVDInputStreamFile> OpenFile(const CFileItem &file);
  int CheckFile(const CFileItem &file, OpenedSegmentMap &result, std::set<MatroskaSegmentUID> &neededSegments);

public:
  int FindSegments(OpenedSegmentMap &result, std::set<MatroskaSegmentUID> &neededSegments, const std::string &srcDir);
  void AddSegment(const MatroskaSegmentUID &uid, const std::string &filename, int64_t offset, bool linked = false);
};

// vim: ts=2 sw=2 expandtab
