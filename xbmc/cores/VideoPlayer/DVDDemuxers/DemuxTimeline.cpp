#include "DemuxTimeline.h"

CDemuxTimeline::CDemuxTimeline(CDVDDemux *pRelayedDemuxer) : m_pRelayedDemuxer(pRelayedDemuxer) {}

CDemuxTimeline::~CDemuxTimeline()
{
  delete m_pRelayedDemuxer;
}

void CDemuxTimeline::Reset()
{
  this->m_pRelayedDemuxer->Reset();
}

void CDemuxTimeline::Abort()
{
  this->m_pRelayedDemuxer->Abort();
}

void CDemuxTimeline::Flush()
{
  this->m_pRelayedDemuxer->Flush();
}

DemuxPacket* CDemuxTimeline::Read()
{
  return this->m_pRelayedDemuxer->Read();
}

bool CDemuxTimeline::SeekTime(int time, bool backwords, double* startpts)
{
  //return false;
  return this->m_pRelayedDemuxer->SeekTime(time, backwords, startpts);
}

bool CDemuxTimeline::SeekChapter(int chapter, double* startpts)
{
  return this->m_pRelayedDemuxer->SeekChapter(chapter, startpts);
}

int CDemuxTimeline::GetChapterCount()
{
  return this->m_pRelayedDemuxer->GetChapterCount();
}

int CDemuxTimeline::GetChapter()
{
  return this->m_pRelayedDemuxer->GetChapter();
}

void CDemuxTimeline::GetChapterName(std::string& strChapterName, int chapterIdx)
{
  this->m_pRelayedDemuxer->GetChapterName(strChapterName, chapterIdx);
}

int64_t CDemuxTimeline::GetChapterPos(int chapterIdx)
{
  return this->m_pRelayedDemuxer->GetChapterPos(chapterIdx);
}

void CDemuxTimeline::SetSpeed(int iSpeed)
{
  this->m_pRelayedDemuxer->SetSpeed(iSpeed);
}

int CDemuxTimeline::GetStreamLength()
{
  return this->m_pRelayedDemuxer->GetStreamLength();
}

std::vector<CDemuxStream*> CDemuxTimeline::GetStreams() const
{
  return this->m_pRelayedDemuxer->GetStreams();
}

int CDemuxTimeline::GetNrOfStreams() const
{
  return this->m_pRelayedDemuxer->GetNrOfStreams();
}

std::string CDemuxTimeline::GetFileName()
{
  return this->m_pRelayedDemuxer->GetFileName();
}

void CDemuxTimeline::EnableStream(int id, bool enable)
{
  return this->m_pRelayedDemuxer->EnableStream(this->m_pRelayedDemuxer->GetDemuxerId(), id, enable);
}

CDemuxStream* CDemuxTimeline::GetStream(int iStreamId) const
{
  return this->m_pRelayedDemuxer->GetStream(this->m_pRelayedDemuxer->GetDemuxerId(), iStreamId);
}

std::string CDemuxTimeline::GetStreamCodecName(int iStreamId)
{
  return this->m_pRelayedDemuxer->GetStreamCodecName(this->m_pRelayedDemuxer->GetDemuxerId(), iStreamId);
}

//int CDemuxTimeline::GetNrOfStreams(StreamType streamType)
//{
//  return this->m_pRelayedDemuxer->GetNrOfStreams(this->m_pRelayedDemuxer->GetDemuxerId(), streamType);
//}


// vim: ts=2 sw=2 expandtab
