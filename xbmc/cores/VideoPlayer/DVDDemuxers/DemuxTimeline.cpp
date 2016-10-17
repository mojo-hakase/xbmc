#include "DemuxTimeline.h"

#include <algorithm>

#include "DVDClock.h"
#include "DVDDemuxPacket.h"
#include "DVDFactoryDemuxer.h"
#include "DVDDemuxFFmpeg.h"
#include "DVDInputStreams/DVDInputStreamFile.h"
#include "DVDInputStreams/DVDFactoryInputStream.h"
#include "filesystem/File.h"
#include "filesystem/Directory.h"
#include "MatroskaParser.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

CDemuxTimeline::CDemuxTimeline() {}

CDemuxTimeline::~CDemuxTimeline() {}

bool CDemuxTimeline::SwitchToNextDemuxer()
{
  if (m_curChapter->index + 1 == m_chapters.size())
    return false;
  CLog::Log(LOGDEBUG, "TimelineDemuxer: Switch Demuxer");
  m_curChapter = &m_chapters[m_curChapter->index + 1];
  m_curChapter->demuxer->SeekTime(m_curChapter->startSrcTime, true);
  return true;
}

void CDemuxTimeline::Reset()
{
  for (auto &demuxer : m_demuxer)
    demuxer->Reset();
  m_curChapter = m_chapterMap.begin()->second;
  if (m_curChapter->startSrcTime != 0)
    m_curChapter->demuxer->SeekTime(m_curChapter->startSrcTime);
}

void CDemuxTimeline::Abort()
{
  m_curChapter->demuxer->Abort();
}

void CDemuxTimeline::Flush()
{
  m_curChapter->demuxer->Flush();
}

#define GET_PTS(x) ((x)->dts!=DVD_NOPTS_VALUE ? (x)->dts : (x)->pts)

DemuxPacket* CDemuxTimeline::Read()
{
  DemuxPacket *packet = nullptr;
  double pts = std::numeric_limits<double>::infinity();
  double dispPts;

  packet = m_curChapter->demuxer->Read();
  packet && (pts = GET_PTS(packet));
  while (
    !packet ||
    pts + packet->duration < DVD_MSEC_TO_TIME(m_curChapter->startSrcTime) ||
    pts >= DVD_MSEC_TO_TIME(m_curChapter->stopSrcTime())
  )
  {
    if (!packet || pts >= DVD_MSEC_TO_TIME(m_curChapter->stopSrcTime()))
      if (!SwitchToNextDemuxer())
        return nullptr;
    packet = m_curChapter->demuxer->Read();
    packet && (pts = GET_PTS(packet));
  }

  dispPts = pts + DVD_MSEC_TO_TIME(m_curChapter->shiftTime());
  packet->dts = dispPts;
  packet->pts = dispPts;
  packet->duration = std::min(packet->duration, DVD_MSEC_TO_TIME(m_curChapter->stopSrcTime()) - pts);
  packet->dispTime = DVD_TIME_TO_MSEC(pts) + m_curChapter->shiftTime();

  return packet;
}

bool CDemuxTimeline::SeekTime(int time, bool backwords, double* startpts)
{
  auto it = m_chapterMap.lower_bound(time);
  if (it == m_chapterMap.end())
    return false;

  CLog::Log(LOGDEBUG, "TimelineDemuxer: Switch Demuxer");
  m_curChapter = it->second;
  bool result = m_curChapter->demuxer->SeekTime(time - m_curChapter->shiftTime(), backwords, startpts);
  if (result && startpts)
    (*startpts) += m_curChapter->shiftTime();
  return result;
}

bool CDemuxTimeline::SeekChapter(int chapter, double* startpts)
{
  --chapter;
  if (chapter < 0 || unsigned(chapter) >= m_chapters.size())
    return false;
  CLog::Log(LOGDEBUG, "TimelineDemuxer: Switch Demuxer");
  m_curChapter = &m_chapters[chapter];
  bool result = m_curChapter->demuxer->SeekTime(m_curChapter->startSrcTime, true, startpts);
  if (result && startpts)
    (*startpts) += m_curChapter->shiftTime();
  return result;
}

int CDemuxTimeline::GetChapterCount()
{
  return m_chapters.size();
}

int CDemuxTimeline::GetChapter()
{
  return m_curChapter->index + 1;
}

void CDemuxTimeline::GetChapterName(std::string& strChapterName, int chapterIdx)
{
  --chapterIdx;
  if (chapterIdx < 0 || unsigned(chapterIdx) >= m_chapters.size())
    return;
  strChapterName = m_chapters[chapterIdx].title;
}

int64_t CDemuxTimeline::GetChapterPos(int chapterIdx)
{
  --chapterIdx;
  if (chapterIdx < 0 || unsigned(chapterIdx) >= m_chapters.size())
    return 0;
  return (m_chapters[chapterIdx].startDispTime + 999) / 1000;
}

void CDemuxTimeline::SetSpeed(int iSpeed)
{
  for (auto &demuxer : m_demuxer)
    demuxer->SetSpeed(iSpeed);
}

int CDemuxTimeline::GetStreamLength()
{
  return m_chapterMap.rbegin()->first;
}

std::vector<CDemuxStream*> CDemuxTimeline::GetStreams() const
{
  return m_primaryDemuxer->GetStreams();
}

int CDemuxTimeline::GetNrOfStreams() const
{
  return m_primaryDemuxer->GetNrOfStreams();
}

std::string CDemuxTimeline::GetFileName()
{
  return m_primaryDemuxer->GetFileName();
}

void CDemuxTimeline::EnableStream(int id, bool enable)
{
  for (auto &demuxer : m_demuxer)
    demuxer->EnableStream(demuxer->GetDemuxerId(), id, enable);
}

CDemuxStream* CDemuxTimeline::GetStream(int iStreamId) const
{
  return m_primaryDemuxer->GetStream(m_primaryDemuxer->GetDemuxerId(), iStreamId);
}

std::string CDemuxTimeline::GetStreamCodecName(int iStreamId)
{
  return m_primaryDemuxer->GetStreamCodecName(m_primaryDemuxer->GetDemuxerId(), iStreamId);
}


CDemuxTimeline* CDemuxTimeline::CreateTimeline(CDVDDemux *demuxer)
{
  return CreateTimelineFromMatroskaParser(demuxer);
  return CreateTimelineFromEbml(demuxer);
}

CDemuxTimeline* CDemuxTimeline::CreateTimelineFromMatroskaParser(CDVDDemux *demuxer)
{
  MatroskaFile mkv;
  return nullptr;
}


/*
#define EBML_ID_HEADER 0x1A45DFA3
#define EBML_ID_VOID 0xEC

#define MATROSKA_ID_SEGMENT 0x18538067

#define MATROSKA_ID_SEEKHEAD 0x114D9B74

#define MATROSKA_ID_INFO 0x1549A966
#define MATROSKA_ID_SEGMENTUID 0x73A4

#define MATROSKA_ID_CHAPTERS 0x1043A770
#define MATROSKA_ID_EDITIONENTRY 0x45B9
#define MATROSKA_ID_EDITIONFLAGORDERED 0x45DD
#define MATROSKA_ID_CHAPTERATOM 0xB6
#define MATROSKA_ID_CHAPTERTIMESTART 0x91
#define MATROSKA_ID_CHAPTERTIMEEND 0x92
#define MATROSKA_ID_CHAPTERSEGMENTUID 0x6E67
#define MATROSKA_ID_CHAPTERDISPLAY 0x80
#define MATROSKA_ID_CHAPTERSTRING 0x85

#define MATROSKA_ID_SEEK 0x4DBB
#define MATROSKA_ID_SEEKID 0x53AB
#define MATROSKA_ID_SEEKPOS 0x53AC

struct MatroskaFile;
struct EbmlHeader;
struct MatroskaSegment;
struct MatroskaSeekHead;
struct MatroskaSegmentInfo;
struct MatroskaChapters;
struct MatroskaEditionEntry;
struct MatroskaChapterAtom;

typedef std::string MatroskaSegmentUID;
struct MatroskaSegmentInfo
{
  bool parsed = false;
  MatroskaSegmentUID uid;
};
struct MatroskaChapterAtom
{
  uint64_t uid;
  int timeStart, timeEnd;
  bool flagHidden, flagEnabled;
  MatroskaSegmentUID segUid;
  std::string title;
  std::map<std::string,std::string> titles;
};
struct MatroskaEditionEntry
{
  uint64_t uid;
  bool flagHidden, flagDefault, flagOrdered;
  std::list<MatroskaChapterAtom> chapterAtoms;
};
struct MatroskaChapters
{
  bool parsed = false;
  std::list<MatroskaEditionEntry> editions;
};
struct MatroskaSeekHead
{
  bool parsed = false;
  std::map<uint32_t,int64_t> seekPosition;
};
struct MatroskaSegment
{
  MatroskaSeekHead seekHead;
  MatroskaSegmentInfo segInfo;
  MatroskaChapters chapters;
};
struct EbmlHeader
{
  uint32_t ebmlVersion;
  uint32_t ebmlReadVersion;
  std::string docType;
  uint32_t docTypeVersion;
  uint32_t docTypeReadVersion;
};
struct MatroskaFile
{
  EbmlHeader ebmlHeader;
  MatroskaSegment segment;

  int64_t offset;
  int64_t segOff;
  uint64_t segLen;
};

uint32_t ReadEbmlTagID(CDVDInputStream *input)
{
  uint32_t id;
  uint8_t first, byte;
  input->Read(&first, 1);
  id = first;
  for (int mask = 1 << 7; mask >= (1 << 5) && (first & mask) == 0; mask >>= 1)
  {
    if (!input->Read(&byte, 1))
      return -1;
    id <<= 8;
    id |= byte;
  }
  return id;
}

uint64_t ReadEbmlTagLen(CDVDInputStream *input)
{
  uint64_t value;
  uint8_t first, byte;
  int len;
  input->Read(&first, 1);
  for (len = 1; len < 8 && (first & (1 << (8 - len))) == 0; ++len);
  value = first & ~(1 << (8 - len));
  while (--len)
  {
    if (!input->Read(&byte, 1))
      return -1;
    value <<= 8;
    value |= byte;
  }
  return value;
}

bool ParseMatroskaSegmentInfo(MatroskaFile *mkv, int64_t tagEnd, CDVDInputStream *input)
{
  while (!input->IsEOF() && input->Seek(0, SEEK_CUR) < tagEnd)
  {
    uint32_t id = ReadEbmlTagID(input);
    uint64_t len = ReadEbmlTagLen(input);
    if (id == MATROSKA_ID_SEGMENTUID && len == 16)
    {
      mkv->segment.segInfo.uid.resize(len);
      input->Read((uint8_t*)mkv->segment.segInfo.uid.data(), 16);
      break;
    }
    else
      input->Seek(len, SEEK_CUR);
  }
  input->Seek(tagEnd, SEEK_SET);
  mkv->segment.segInfo.parsed = true;
  return true;
}

bool ParseMatroskaSeekHead(MatroskaFile *mkv, int64_t tagEnd, CDVDInputStream *input)
{
  uint32_t seekID = 0;
  int64_t seekPos = 0;
  uint8_t byte;
  while (!input->IsEOF() && input->Seek(0, SEEK_CUR) < tagEnd)
  {
    uint32_t id = ReadEbmlTagID(input);
    uint64_t len = ReadEbmlTagLen(input);
    if (id == MATROSKA_ID_SEEKID && len <= 4)
      for (seekID = 0; len > 0 && input->Read(&byte, 1); --len)
        seekID = (seekID << 8) | byte;
    else if (id == MATROSKA_ID_SEEKPOS && len <= 8 && seekID)
    {
      for (seekPos = 0; len > 0 && input->Read(&byte, 1); --len)
        seekPos = (seekPos << 8) | byte;
      mkv->segment.seekHead.seekPosition[seekID] = seekPos;
      seekID = 0;
    }
    else if (id != MATROSKA_ID_SEEK)
    {
      seekID = 0;
      input->Seek(len, SEEK_CUR);
    }
  }
  input->Seek(tagEnd, SEEK_SET);
  mkv->segment.seekHead.parsed = true;
  return true;
}

bool ParseMatroskaChapterAtom(MatroskaEditionEntry *edition, int64_t tagEnd, CDVDInputStream *input)
{
  edition->chapterAtoms.emplace_back();
  MatroskaChapterAtom &atom = edition->chapterAtoms.back();
  while (!input->IsEOF() && input->Seek(0, SEEK_CUR) < tagEnd)
  {
    uint32_t id = ReadEbmlTagID(input);
    uint64_t len = ReadEbmlTagLen(input);
    uint8_t byte;
    uint64_t uval;
    switch (id)
    {
      case MATROSKA_ID_CHAPTERTIMESTART:
        for (uval = 0; len > 0 && input->Read(&byte, 1); --len)
          uval = (uval << 8) | byte;
        uval /= 1000000;
        atom.timeStart = int(uval);
        break;
      case MATROSKA_ID_CHAPTERTIMEEND:
        for (uval = 0; len > 0 && input->Read(&byte, 1); --len)
          uval = (uval << 8) | byte;
        uval /= 1000000;
        atom.timeEnd = int(uval);
        break;
      case MATROSKA_ID_CHAPTERSEGMENTUID:
        atom.segUid.resize((len < 16) ? len : 16);
        input->Read((uint8_t*)atom.segUid.data(), atom.segUid.size());
        if (len > 16)
          input->Seek(len - 16, SEEK_CUR);
        break;
      case MATROSKA_ID_CHAPTERDISPLAY:
        break;
      case MATROSKA_ID_CHAPTERSTRING:
        atom.title.resize((len < 0x1000) ? len : 0x1000);
        input->Read((uint8_t*)atom.title.data(), atom.title.size());
        if (len > atom.title.size())
          input->Seek(len - atom.title.size(), SEEK_CUR);
        break;
      default:
        input->Seek(len, SEEK_CUR);
    }
  }
  return true;
}

bool ParseMatroskaEditionEntry(MatroskaFile *mkv, int64_t tagEnd, CDVDInputStream *input)
{
  mkv->segment.chapters.editions.emplace_back();
  MatroskaEditionEntry &edition = mkv->segment.chapters.editions.back();
  while (!input->IsEOF() && input->Seek(0, SEEK_CUR) < tagEnd)
  {
    uint32_t id = ReadEbmlTagID(input);
    uint64_t len = ReadEbmlTagLen(input);
    uint8_t byte;
    switch (id)
    {
      case MATROSKA_ID_EDITIONFLAGORDERED:
        input->Seek(len-1, SEEK_CUR);
        input->Read(&byte, 1);
        edition.flagOrdered = byte;
        break;
      case MATROSKA_ID_CHAPTERATOM:
        if (!ParseMatroskaChapterAtom(&edition, input->Seek(0, SEEK_CUR) + len, input))
          return false;
        break;
      default:
        input->Seek(len, SEEK_CUR);
    }
  }
  return true;
}

bool ParseMatroskaChapters(MatroskaFile *mkv, int64_t tagEnd, CDVDInputStream *input)
{
  while (!input->IsEOF() && input->Seek(0, SEEK_CUR) < tagEnd)
  {
    uint32_t id = ReadEbmlTagID(input);
    uint64_t len = ReadEbmlTagLen(input);
    if (id == MATROSKA_ID_EDITIONENTRY)
      ParseMatroskaEditionEntry(mkv, input->Seek(0, SEEK_CUR) + len, input);
    else
      input->Seek(len, SEEK_CUR);
  }
  input->Seek(tagEnd, SEEK_SET);
  mkv->segment.chapters.parsed = true;
  return true;
}

bool ParseMatroskaFile(MatroskaFile *mkv, CDVDInputStream *input, bool parseChapters = false)
{
  mkv->offset = input->Seek(0, SEEK_CUR);

  int64_t id, len;
  id = ReadEbmlTagID(input);
  if (id != EBML_ID_HEADER)
    return false;
  len = ReadEbmlTagLen(input);
  input->Seek(len, SEEK_CUR);
  id = ReadEbmlTagID(input);
  if (id != MATROSKA_ID_SEGMENT)
    return false;
  mkv->segLen = ReadEbmlTagLen(input);
  mkv->segOff = input->Seek(0, SEEK_CUR);

  while (!input->IsEOF() && !(
    mkv->segment.seekHead.parsed ||
    (mkv->segment.segInfo.parsed && (mkv->segment.chapters.parsed || !parseChapters))
  ))
  {
    id = ReadEbmlTagID(input);
    len = ReadEbmlTagLen(input);
    switch (id)
    {
      case MATROSKA_ID_INFO:
        if (!ParseMatroskaSegmentInfo(mkv, input->Seek(0, SEEK_CUR) + len, input))
          return false;
        break;
      case MATROSKA_ID_SEEKHEAD:
        if (!ParseMatroskaSeekHead(mkv, input->Seek(0, SEEK_CUR) + len, input))
          return false;
        break;
      case MATROSKA_ID_CHAPTERS:
        if (parseChapters && !ParseMatroskaChapters(mkv, input->Seek(0, SEEK_CUR) + len, input))
          return false;
        break;
      default:
        input->Seek(len, SEEK_CUR);
        break;
    }
  }

  if (input->IsEOF())
    return false;

  if (!mkv->segment.segInfo.parsed)
  {
    auto it = mkv->segment.seekHead.seekPosition.find(MATROSKA_ID_INFO);
    if (it == mkv->segment.seekHead.seekPosition.end())
      return false;
    input->Seek(mkv->segOff + it->second, SEEK_SET);
    id = ReadEbmlTagID(input);
    len = ReadEbmlTagLen(input);
    if (id != MATROSKA_ID_INFO)
      return false;
    if (!ParseMatroskaSegmentInfo(mkv, input->Seek(0, SEEK_CUR) + len, input))
      return false;
  }
  if (parseChapters && !mkv->segment.chapters.parsed)
  {
    auto it = mkv->segment.seekHead.seekPosition.find(MATROSKA_ID_CHAPTERS);
    if (it == mkv->segment.seekHead.seekPosition.end())
      return true;
    input->Seek(mkv->segOff + it->second, SEEK_SET);
    id = ReadEbmlTagID(input);
    len = ReadEbmlTagLen(input);
    if (id != MATROSKA_ID_CHAPTERS)
      return true;
    if (!ParseMatroskaChapters(mkv, input->Seek(0, SEEK_CUR) + len, input))
      return true;
  }

  return true;
}

CDemuxTimeline* CDemuxTimeline::CreateTimelineFromEbml(CDVDDemux *primaryDemuxer)
{
  std::unique_ptr<CDVDInputStreamFile> inStream(new CDVDInputStreamFile(CFileItem(primaryDemuxer->GetFileName(), false)));
  if (!inStream->Open())
    return nullptr;
  CDVDInputStream *input = inStream.get();

  MatroskaFile matroskaFile;
  MatroskaFile *mkv = &matroskaFile;
  if (!ParseMatroskaFile(mkv, input, true))
    return nullptr;

  if (!mkv->segment.chapters.editions.size())
    return nullptr;

  MatroskaEditionEntry &edition = mkv->segment.chapters.editions.front();
  if (!edition.flagOrdered)
    return nullptr;

  std::unique_ptr<CDemuxTimeline> timeline(new CDemuxTimeline);
  timeline->m_primaryDemuxer = primaryDemuxer;
  timeline->m_demuxer.emplace_back(primaryDemuxer);
  std::map<MatroskaSegmentUID,CDVDDemux*> extDemuxer;
  std::set<MatroskaSegmentUID> missingSegments;
  for (auto &chapter : edition.chapterAtoms)
    if (chapter.segUid.size())
      missingSegments.insert(chapter.segUid);
  extDemuxer[mkv->segment.segInfo.uid] = primaryDemuxer;
  extDemuxer[MatroskaSegmentUID()] = primaryDemuxer;

  // find extern segments
  std::string filename = primaryDemuxer->GetFileName();
  std::string dirname = filename.substr(0, filename.rfind('/') + 1);
  CFileItemList files;
  XFILE::CDirectory::GetDirectory(dirname, files, ".mkv");
  for (auto &file : files.GetList())
  {
    timeline->m_inputStreams.emplace_back(CDVDFactoryInputStream::CreateInputStream(nullptr, *file));
    CDVDInputStream *input = timeline->m_inputStreams.back().get();
    if (!input)
      continue;
    if (!input->Open())
      continue;
    MatroskaFile mkv2;
    while (missingSegments.size() && ParseMatroskaFile(&mkv2, input))
    {
      auto it = missingSegments.find(mkv2.segment.segInfo.uid);
      if (it != missingSegments.end())
      {
        input->Seek(mkv2.offset, SEEK_SET);
        std::unique_ptr<CDVDDemuxFFmpeg> demuxer(new CDVDDemuxFFmpeg());
        if(demuxer->Open(input))
        {
          missingSegments.erase(it);
          extDemuxer[mkv2.segment.segInfo.uid] = demuxer.get();
          timeline->m_demuxer.emplace_back(demuxer.release());
          timeline->m_inputStreams.emplace_back(CDVDFactoryInputStream::CreateInputStream(nullptr, *file));
          input = timeline->m_inputStreams.back().get();
          if (!input)
            break;
          if (!input->Open())
            break;
        }
      }
      input->Seek(mkv2.segOff + mkv2.segLen, SEEK_SET);
      mkv2 = MatroskaFile();
    }
    timeline->m_inputStreams.pop_back();
    if (missingSegments.size() == 0)
      break;
  }

  for (auto &segUid : missingSegments)
    CLog::Log(LOGERROR,
      "TimelineDemuxer: Could not find matroska segment for segment linking: %llu",
      segUid.data());

  int dispTime = 0;
  decltype(extDemuxer.begin()) it;
  for (auto &chapter : edition.chapterAtoms)
    if ((it = extDemuxer.find(chapter.segUid)) != extDemuxer.end())
    {
      timeline->m_chapters.emplace_back(
        it->second,
        chapter.timeStart,
        dispTime,
        (chapter.timeEnd - chapter.timeStart),
        timeline->m_chapters.size(),
        chapter.title
      );
      dispTime += timeline->m_chapters.back().duration;
    }

  if (!timeline->m_chapters.size())
    return nullptr;

  for (auto &chapter : timeline->m_chapters)
    timeline->m_chapterMap[chapter.stopDispTime() - 1] = &chapter;

  timeline->m_curChapter = &timeline->m_chapters.front();
  return timeline.release();
}
*/

// vim: ts=2 sw=2 expandtab
