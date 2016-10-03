#pragma once

#include "DVDDemux.h"

#include <list>
#include <map>
#include <memory>

class CDemuxTimeline : public CDVDDemux
{

public:
  CDemuxTimeline();
  virtual ~CDemuxTimeline();

  static CDemuxTimeline* CreateTimeline(CDVDDemux *primaryDemuxer/*, AVFormatContext *avfc*/);

  /*
   * Reset the entire demuxer (same result as closing and opening it)
   */
  void Reset() override;

  /*
   * Aborts any internal reading that might be stalling main thread
   * NOTICE - this can be called from another thread
   */
  void Abort() override;

  /*
   * Flush the demuxer, if any data is kept in buffers, this should be freed now
   */
  void Flush() override;

  /*
   * Read a packet, returns NULL on error
   *
   */
  DemuxPacket* Read() override;

  /*
   * Seek, time in msec calculated from stream start
   */
  bool SeekTime(int time, bool backwords = false, double* startpts = NULL) override;

  /*
   * Seek to a specified chapter.
   * startpts can be updated to the point where display should start
   */
  bool SeekChapter(int chapter, double* startpts = NULL) override;

  /*
   * Get the number of chapters available
   */
  int GetChapterCount() override;

  /*
   * Get current chapter
   */
  int GetChapter() override;

  /*
   * Get the name of a chapter
   * \param strChapterName[out] Name of chapter
   * \param chapterIdx -1 for current chapter, else a chapter index
   */
  void GetChapterName(std::string& strChapterName, int chapterIdx=-1) override;

  /*
   * Get the position of a chapter
   * \param chapterIdx -1 for current chapter, else a chapter index
   */
  int64_t GetChapterPos(int chapterIdx=-1) override;

  /*
   * Set the playspeed, if demuxer can handle different
   * speeds of playback
   */
  void SetSpeed(int iSpeed) override;

  /*
   * returns the total time in msec
   */
  int GetStreamLength() override;

  std::vector<CDemuxStream*> GetStreams() const override;

  /*
   * return nr of streams, 0 if none
   */
  int GetNrOfStreams() const override;

  /*
   * returns opened filename
   */
  std::string GetFileName() override;


  void EnableStream(int id, bool enable) override;
  CDemuxStream* GetStream(int iStreamId) const override;
  std::string GetStreamCodecName(int iStreamId) override;

private:
  bool SwitchToNextDemuxer();

  struct DemuxerInfo {
    double nextPts;
    //CDemuxPacket *pLastDemuxPacket;
    bool sendLastPacket;
  };

  struct ChapterInfo {
    CDVDDemux *demuxer;
    int startSrcTime;  // in MSEC
    int startDispTime; // in MSEC
    int duration; // in MSEC
    int stopSrcTime() {return startSrcTime + duration;}
    int stopDispTime() {return startDispTime + duration;}
    int shiftTime() {return startDispTime - startSrcTime;}
    unsigned int index;
    std::string title;
  };

  CDVDDemux *m_primaryDemuxer;

  //std::list<std::shared_ptr<CDVDDemux>> m_demuxer;
  std::list<std::shared_ptr<CDVDDemux>> m_demuxer;
  std::map<CDVDDemux*,DemuxerInfo> m_demuxerInfos;

  std::vector<ChapterInfo> m_chapters;
  std::map<int,ChapterInfo*> m_chapterMap;  // maps chapter end display time in msec to chapter info

  ChapterInfo *m_curChapter;
  double m_lastDispPts;
};

// vim: ts=2 sw=2 expandtab
