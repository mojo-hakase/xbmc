#include "DemuxTimeline.h"

#include <algorithm>
#include <iostream>
#include <memory>

#include "DVDDemuxPacket.h"
#include "DVDFactoryDemuxer.h"
#include "DVDInputStreams/DVDInputStreamFile.h"
#include "DVDInputStreams/DVDFactoryInputStream.h"
#include "filesystem/File.h"
#include "utils/StringUtils.h"
#include "../DVDClock.h"

CDemuxTimeline::CDemuxTimeline() {}

CDemuxTimeline::~CDemuxTimeline() {}

CDemuxTimeline* CDemuxTimeline::CreateTimeline(CDVDDemux *demuxer/*, AVFormatContext *avfc*/)
{
  std::string filename = demuxer->GetFileName();
  std::string csvFilename = filename + ".OrderedChapters.csv";
  if (!XFILE::CFile::Exists(csvFilename))
    return nullptr;

  std::string dirname = filename.substr(0, filename.rfind('/') + 1);
  
  CDemuxTimeline *timeline = new CDemuxTimeline;
  timeline->m_pPrimaryDemuxer = demuxer;
  timeline->m_demuxer.emplace_front(demuxer);

  int timelineLen = 0;
  int index = 0;
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
    int dispTime = timelineLen;
    timelineLen += (endTime - startTime);

    DemuxerInfo *chapterDemuxInfo = &timeline->m_demuxer.front();
    std::string strLine(szLine);
    size_t pos = strLine.find(',', 0);
    pos = strLine.find(',', pos + 1);
    pos++;
    std::string sourceFilename = strLine.substr(pos);
    if (StringUtils::Trim(sourceFilename).size())
    {
      //CDVDInputStream *inStream = new CDVDInputStreamFile(CFileItem(dirname + sourceFilename, false));
      CDVDInputStream *inStream = CDVDFactoryInputStream::CreateInputStream(nullptr, CFileItem(dirname + sourceFilename, false)); 
      inStream->Open();
      CDVDDemux *chapterDemux = CDVDFactoryDemuxer::CreateDemuxer(inStream);
      if (!chapterDemux)
        continue;
      timeline->m_demuxer.emplace_back(chapterDemux);
      chapterDemuxInfo = &timeline->m_demuxer.back();
    }

    ChapterInfo &newChapter = timeline->m_chapters[timelineLen];
    newChapter.startSrcTime = startTime;
    newChapter.startDispTime = dispTime;
    newChapter.duration = (endTime - startTime);
    newChapter.pDemuxerInfo = chapterDemuxInfo;
    newChapter.index = index++;
  }

  timeline->m_pCurChapter = &timeline->m_chapters.begin()->second;
  timeline->m_pCurDemuxInfo = timeline->m_pCurChapter->pDemuxerInfo;
  return timeline;
}

void CDemuxTimeline::SwitchToNextDemuxer()
{
  auto it = m_chapters.lower_bound(m_pCurChapter->stopDispTime());
  ++it;
  m_pCurChapter = &it->second;
  m_pCurDemuxInfo = m_pCurChapter->pDemuxerInfo;
  //m_pCurDemuxInfo->pDemuxer->SeekTime(m_pCurChapter->startSrcTime);
  m_pCurDemuxInfo->pDemuxer->SeekTime(m_pCurChapter->startSrcTime, true);
}

void CDemuxTimeline::Reset()
{
  for (auto &demuxer : m_demuxer)
    demuxer.pDemuxer->Reset();
  m_pCurChapter = &m_chapters.begin()->second;
  m_pCurDemuxInfo = m_pCurChapter->pDemuxerInfo;
  if (m_pCurChapter->startSrcTime != 0)
    m_pCurDemuxInfo->pDemuxer->SeekTime(m_pCurChapter->startSrcTime);
}

void CDemuxTimeline::Abort()
{
  m_pCurDemuxInfo->pDemuxer->Abort();
}

void CDemuxTimeline::Flush()
{
  m_pCurDemuxInfo->pDemuxer->Flush();
}

void dump_packet_info(DemuxPacket *packet)
{
  std::cout << "packet:" << std::endl;
  std::cout << "\tdts:\t" << packet->dts << std::endl;
  std::cout << "\tpts:\t" << packet->pts << std::endl;
  std::cout << "\tduration:\t" << packet->duration << std::endl;
  std::cout << "\tdispTime:\t" << packet->dispTime << std::endl;
  std::cout << "\tdemuxerIds:\t" << packet->demuxerId << std::endl;
  std::cout << "\tiStreamId:\t" << packet->iStreamId << std::endl;
  std::cout << "\tiGroupId:\t" << packet->iGroupId << std::endl;
}

DemuxPacket* CDemuxTimeline::Read()
{
#define CHAPTERED_MODE
  CDVDDemux *demuxer;
  DemuxPacket *packet;
#ifdef CHAPTERED_MODE
  double pts = 0;
#else
  static double pts = 0;
  static double startDispPts = 0;
  static double duration = 0;
#endif
  double dispPts;

  //if (!m_pCurDemuxInfo || !m_pCurDemuxInfo->pDemuxer)
  //  packet = nullptr;
  demuxer = m_pCurDemuxInfo->pDemuxer;
  packet = demuxer->Read();
#ifdef CHAPTERED_MODE
  //if (!packet)
  //  return nullptr;
#endif

  if (packet)
  {
    pts = packet->dts != DVD_NOPTS_VALUE ? packet->dts : packet->pts;
#ifndef CHAPTERED_MODE
    duration = packet->duration;
#endif
  }
#ifdef CHAPTERED_MODE
  while (pts >= DVD_MSEC_TO_TIME(m_pCurChapter->stopSrcTime()))
#else
  while (!packet)
#endif
  {
    //m_pCurDemuxInfo->pLastDemuxPacket = packet;
    std::cout << "SWITCH DEMUXER" << std::endl;
    SwitchToNextDemuxer();
    demuxer = m_pCurDemuxInfo->pDemuxer;
    packet = demuxer->Read();
    if (!packet)
      return nullptr;
#ifndef CHAPTERED_MODE
    startDispPts = pts + duration;
#endif
    pts = packet->dts != DVD_NOPTS_VALUE ? packet->dts : packet->pts;
  }
#ifdef CHAPTERED_MODE
  dispPts = pts + DVD_MSEC_TO_TIME(m_pCurChapter->shiftTime());
  packet->dts = dispPts; // probably shouldn't be done
  packet->pts = dispPts; // probably shouldn't be done
  packet->duration = std::min(packet->duration, DVD_MSEC_TO_TIME(m_pCurChapter->stopSrcTime()) - pts);
  packet->dispTime = DVD_TIME_TO_MSEC(pts) + m_pCurChapter->shiftTime();
#else
  dispPts = pts + startDispPts;
  packet->dts = dispPts; // probably shouldn't be done
  packet->pts = dispPts; // probably shouldn't be done
  packet->dispTime = DVD_TIME_TO_MSEC(dispPts);
#endif
  packet->demuxerId = this->GetDemuxerId();

  dump_packet_info(packet);

  return packet;
}

bool CDemuxTimeline::SeekTime(int time, bool backwords, double* startpts)
{
  auto it = m_chapters.lower_bound(time);
  if (it == m_chapters.end())
    return false;
  m_pCurChapter = &it->second;
  m_pCurDemuxInfo = m_pCurChapter->pDemuxerInfo;
  return m_pCurChapter->pDemuxerInfo->pDemuxer->SeekTime(time - m_pCurChapter->shiftTime(), backwords, startpts);
}

bool CDemuxTimeline::SeekChapter(int chapter, double* startpts)
{
  auto it = m_chapters.begin();
  for (int i = 0; i != chapter && it != m_chapters.end(); ++it, ++i);
  if (it != m_chapters.end())
    m_pCurChapter = &it->second;
  return m_pCurChapter->pDemuxerInfo->pDemuxer->SeekTime(m_pCurChapter->startSrcTime, true, startpts);
}

int CDemuxTimeline::GetChapterCount()
{
  return m_chapters.size();
}

int CDemuxTimeline::GetChapter()
{
  return m_pCurChapter->index;
}

void CDemuxTimeline::GetChapterName(std::string& strChapterName, int chapterIdx)
{
  auto it = m_chapters.begin();
  for (int i = 0; i != chapterIdx && it != m_chapters.end(); ++it, ++i);
  if (it != m_chapters.end())
    strChapterName = it->second.title;
}

int64_t CDemuxTimeline::GetChapterPos(int chapterIdx)
{
  auto it = m_chapters.begin();
  for (int i = 0; i != chapterIdx && it != m_chapters.end(); ++it, ++i);
  if (it != m_chapters.end())
    return it->second.startDispTime;
  return 0;
}

void CDemuxTimeline::SetSpeed(int iSpeed)
{
  for (auto &demuxer : m_demuxer)
    demuxer.pDemuxer->SetSpeed(iSpeed);
}

int CDemuxTimeline::GetStreamLength()
{
  return m_chapters.rbegin()->first;
}

std::vector<CDemuxStream*> CDemuxTimeline::GetStreams() const
{
  //auto res = m_pCurDemuxInfo->pDemuxer->GetStreams();
  auto res = m_pPrimaryDemuxer->GetStreams();
  decltype(res) cpy;
  for (auto &elem : res)
    //if (elem->type != STREAM_SUBTITLE)
      cpy.push_back(elem);
  return cpy;
}

int CDemuxTimeline::GetNrOfStreams() const
{
  return m_pPrimaryDemuxer->GetNrOfStreams();
}

std::string CDemuxTimeline::GetFileName()
{
  return m_pPrimaryDemuxer->GetFileName();
}

void CDemuxTimeline::EnableStream(int id, bool enable)
{
  for (auto &demuxer : m_demuxer)
    demuxer.pDemuxer->EnableStream(demuxer.pDemuxer->GetDemuxerId(), id, enable);
}

CDemuxStream* CDemuxTimeline::GetStream(int iStreamId) const
{
  return m_pPrimaryDemuxer->GetStream(m_pPrimaryDemuxer->GetDemuxerId(), iStreamId);
}

std::string CDemuxTimeline::GetStreamCodecName(int iStreamId)
{
  return m_pPrimaryDemuxer->GetStreamCodecName(m_pPrimaryDemuxer->GetDemuxerId(), iStreamId);
  //return m_pCurDemuxInfo->pDemuxer->GetStreamCodecName(m_pCurDemuxInfo->pDemuxer->GetDemuxerId(), iStreamId);
}

//int CDemuxTimeline::GetNrOfStreams(StreamType streamType)
//{
//  return m_pCurDemuxInfo->pDemuxer->GetNrOfStreams(m_pCurDemuxInfo->pDemuxer->GetDemuxerId(), streamType);
//}



// vim: ts=2 sw=2 expandtab
