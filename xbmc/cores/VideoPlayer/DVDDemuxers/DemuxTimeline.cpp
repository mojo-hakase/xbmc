#include "DemuxTimeline.h"

#include <algorithm>
#include <iostream>
#include <memory>

#include "DVDClock.h"
#include "DVDDemuxPacket.h"
#include "DVDFactoryDemuxer.h"
#include "DVDInputStreams/DVDInputStream.h"
#include "DVDInputStreams/DVDFactoryInputStream.h"
#include "filesystem/File.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

CDemuxTimeline::CDemuxTimeline() {}

CDemuxTimeline::~CDemuxTimeline() {}

CDemuxTimeline* CDemuxTimeline::CreateTimeline(CDVDDemux *demuxer)
{
  std::string filename = demuxer->GetFileName();
  std::string csvFilename = filename + ".OrderedChapters.csv";
  if (!XFILE::CFile::Exists(csvFilename))
    return nullptr;

  std::string dirname = filename.substr(0, filename.rfind('/') + 1);

  CDemuxTimeline *timeline = new CDemuxTimeline;
  timeline->m_primaryDemuxer = demuxer;
  timeline->m_demuxer.emplace_back(demuxer);
  timeline->m_demuxerInfos[demuxer];

  int timelineLen = 0;
  XFILE::CFile csvFile;
  csvFile.Open(csvFilename);
  int bufferSize = 4096;
  char szLine[bufferSize];
  while (csvFile.ReadString(szLine, bufferSize))
  {
    int sh, sm, ss, sc, eh, em, es, ec;
    if (!sscanf(szLine, "%d:%d:%d.%3d%*[^,],%d:%d:%d.%3d%*[^,],", &sh, &sm, &ss, &sc, &eh, &em, &es, &ec))
      continue;

    int startTime = (((sh * 60) + sm) * 60 + ss) * 1000 + sc;
    int endTime   = (((eh * 60) + em) * 60 + es) * 1000 + ec;
    if (startTime > endTime)
      continue;

    CDVDDemux *chapterDemuxer = timeline->m_primaryDemuxer;
    std::string strLine(szLine);
    size_t pos = strLine.find(',', 0);
    pos = strLine.find(',', pos + 1);
    pos++;
    std::string sourceFilename = strLine.substr(pos);
    if (StringUtils::Trim(sourceFilename).size())
    {
      CDVDInputStream *inStream = CDVDFactoryInputStream::CreateInputStream(nullptr, CFileItem(dirname + sourceFilename, false));
      inStream->Open();
      chapterDemuxer = CDVDFactoryDemuxer::CreateDemuxer(inStream);
      if (!chapterDemuxer)
        continue;
      timeline->m_demuxer.emplace_back(chapterDemuxer);
      timeline->m_demuxerInfos[chapterDemuxer];
    }
    int dispTime = timelineLen;
    timelineLen += (endTime - startTime);

    timeline->m_chapters.emplace_back(ChapterInfo {
      .demuxer = chapterDemuxer,
      .startSrcTime = startTime,
      .startDispTime = dispTime,
      .duration = (endTime - startTime),
      .index = timeline->m_chapters.size()
    });
  }
  for (auto &chapter : timeline->m_chapters)
    timeline->m_chapterMap[chapter.stopDispTime() - 1] = &chapter;

  timeline->m_curChapter = &timeline->m_chapters.front();
  return timeline;
}

bool CDemuxTimeline::SwitchToNextDemuxer()
{
  if (m_curChapter->index + 1 == m_chapters.size())
    return false;
  CLog::Log(LOGDEBUG, "TimelineDemuxer: Switch Demuxer");
  m_curChapter = &m_chapters[m_curChapter->index + 1];
  m_curChapter->demuxer->SeekTime(m_curChapter->startSrcTime);
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
  for (packet && (pts = GET_PTS(packet));
    !packet ||
    pts + packet->duration < DVD_MSEC_TO_TIME(m_curChapter->startSrcTime) ||
    pts >= DVD_MSEC_TO_TIME(m_curChapter->stopSrcTime());
    packet && (pts = GET_PTS(packet)))
  {
    if (!packet || pts >= DVD_MSEC_TO_TIME(m_curChapter->stopSrcTime()))
      if (!SwitchToNextDemuxer())
        return nullptr;
    packet = m_curChapter->demuxer->Read();
  }

  dispPts = pts + DVD_MSEC_TO_TIME(m_curChapter->shiftTime());
  packet->dts = dispPts;
  packet->pts = dispPts;
  packet->duration = std::min(packet->duration, DVD_MSEC_TO_TIME(m_curChapter->stopSrcTime()) - pts);
  packet->dispTime = DVD_TIME_TO_MSEC(pts) + m_curChapter->shiftTime();
  packet->demuxerId = this->GetDemuxerId();

  if (this->GetStream(packet->iStreamId)->type == STREAM_SUBTITLE)
    std::cout << "PACKET: (" << int(packet->pts/1000) << ")-(" << int((packet->pts+packet->duration)/1000) << "), ChapterStart:" << m_curChapter->startDispTime << std::endl << std::string(reinterpret_cast<char*>(packet->pData), packet->iSize) << std::endl;

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
  return m_chapters[chapterIdx].startDispTime / 1000;
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


// vim: ts=2 sw=2 expandtab
