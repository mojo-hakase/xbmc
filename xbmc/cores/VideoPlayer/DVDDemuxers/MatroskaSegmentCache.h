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

// .c file

#include "filesystem/Directory.h"

MatroskaSegmentCache::SharedCacheEntry MatroskaSegmentCache::LookUp(const MatroskaSegmentUID &uid)
{
  auto it = segmentCache.find(uid);
  if (it == segmentCache.end())
    return nullptr;
  // update cache
  SharedCacheEntry entry = it->second;
  cacheList.splice(cacheList.begin(), cacheList, entry->cacheListIterator);
  return entry;
}

std::unique_ptr<CDVDInputStreamFile> MatroskaSegmentCache::OpenSegment(const SegmentLocator &locator)
{
  auto stream = OpenFile(CFileItem(locator.filename, false));
  if (locator.offset && stream)
    stream->Seek(locator.offset, SEEK_SET);
  return stream;
}

std::unique_ptr<CDVDInputStreamFile> MatroskaSegmentCache::OpenFile(const CFileItem &file)
{
  std::unique_ptr<CDVDInputStreamFile> inStream(new CDVDInputStreamFile(file));
  if (!inStream || !inStream->Open())
    return nullptr;
  return inStream;
}

void MatroskaSegmentCache::AddSegment(const MatroskaSegmentUID &uid, const std::string &filename, int64_t offset, bool linked)
{
  auto range = fileCache.equal_range(filename);
  for (auto it = range.first; it != range.second; ++it)
    if (it->second->segmentLocator.offset == offset)
      return;
  SharedCacheEntry entry(new CacheEntry);
  entry->segmentLocator.filename = filename;
  entry->segmentLocator.offset = offset;
  entry->cacheListIterator = cacheList.insert(linked ? cacheList.begin() : cacheList.end(), entry);
  entry->segmentCacheIterator = segmentCache.emplace(uid, entry).first;
  entry->fileCacheIterator = fileCache.emplace(filename, entry);
}

int MatroskaSegmentCache::CheckFile(const CFileItem &file, OpenedSegmentMap &result, std::set<MatroskaSegmentUID> &neededSegments)
{
  auto range = fileCache.equal_range(file.GetPath());
  int count = 0;
  for (auto &it = range.first; it != range.second; ++it)
  {
    auto &uid = it->second->segmentCacheIterator->first;
    if (!neededSegments.erase(uid))
      continue;
    auto fileInput = OpenSegment(it->second->segmentLocator);
    if (!fileInput)
      return count;
    result[uid] = std::move(fileInput);
    ++count;
    // update cache list
    cacheList.splice(cacheList.begin(), cacheList, it->second->cacheListIterator);
  }
  if (range.first != range.second)
    return count;

  auto input = OpenFile(file);
  if (!input)
    return count;
  MatroskaFile mkv;
  if (!mkv.Parse(input.get()))
    return count;
  auto &uid = mkv.segment.infos.uid;
  if (neededSegments.erase(uid))
  {
    input->Seek(mkv.offsetBegin, SEEK_SET);
    result[uid] = std::move(input);
    AddSegment(uid, file.GetPath(), mkv.offsetBegin, true);
  }
  else
    AddSegment(uid, file.GetPath(), mkv.offsetBegin, false);
  return count;
}

int MatroskaSegmentCache::FindSegments(OpenedSegmentMap &result, std::set<MatroskaSegmentUID> &neededSegments, const std::string &srcDir)
{
  for (auto &pair : result)
    neededSegments.erase(pair.first);
  int count = neededSegments.size();
  for (auto next = neededSegments.begin(), it = next++; it != neededSegments.end(); it = next++)
  {
    auto const &uid = *it;
    auto entry = LookUp(uid);
    if (!entry)
      continue;
    auto fileInput = OpenSegment(entry->segmentLocator);
    if (!fileInput)
      continue;
    result[uid] = std::move(fileInput);
    neededSegments.erase(it);
  }
  if (neededSegments.size() == 0)
    return count;
  std::list<std::string> searchDirs({""}); // should be a global setting
  for (auto const &subDir : searchDirs)
  {
    CFileItemList files;
    XFILE::CDirectory::GetDirectory(srcDir + subDir, files, ".mkv");
    for (auto const &file : files.GetList())
    {
      CheckFile(*file, result, neededSegments);
      if (neededSegments.size() == 0)
        break;
    }
    if (neededSegments.size() == 0)
      break;
  }
  return count - neededSegments.size();
}

// vim: ts=2 sw=2 expandtab
