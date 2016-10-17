#pragma once

#include "EbmlParser.h"

using MatroskaSegmentUID = std::string;
using MatroskaChapterDisplayMap = std::map<std::string,std::string>;

struct MatroskaChapterAtom
{
  uint64_t uid = 0;
  int timeStart = 0;
  int timeEnd = 0;
  bool flagHidden = false;
  bool flagEnabled = false;
  MatroskaSegmentUID segUid;
  uint64_t segEditionUid = 0;
  MatroskaChapterDisplayMap displays;
};

struct MatroskaEdition
{
  uint64_t uid = 0;
  bool flagHidden = false;
  bool flagDefault = false;
  bool flagOrdered = false;
  std::list<MatroskaChapterAtom> chapterAtoms;
};

struct MatroskaChapters
{
  std::list<MatroskaEdition> editions;
};

typedef std::multimap<EbmlId,uint64_t> MatroskaSeekMap;

struct MatroskaSegmentInfo
{
  MatroskaSegmentUID uid;
  uint64_t timecodeScale = 1000000;
};

struct MatroskaSegment
{
  int64_t offset;
  MatroskaSegmentInfo infos;
  MatroskaSeekMap seekMap;
  MatroskaChapters chapters;

  bool Parse(CDVDInputStream *input);
};

struct MatroskaFile
{
  int64_t offsetBegin;
  int64_t offsetEnd;
  EbmlHeader ebmlHeader;
  MatroskaSegment segment;
  bool Parse(CDVDInputStream *input);
};

// vim: ts=2 sw=2 expandtab
