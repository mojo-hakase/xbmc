#pragma once

#include "DVDDemux.h"

#include <list>
#include <map>

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

  int GetNrOfStreams(StreamType streamType);

private:
  void SwitchToNextDemuxer();

  CDVDDemux *m_pPrimaryDemuxer;

  struct DemuxerInfo {
    CDVDDemux *pDemuxer;
    double nextPts;
    //CDVDDemux *pLastDemuxPacket;
    bool sendLastPacket;
    DemuxerInfo(CDVDDemux *demuxer) : pDemuxer(demuxer) {}
    ~DemuxerInfo() {delete pDemuxer;}
  };

  struct ChapterInfo {
    DemuxerInfo *pDemuxerInfo;
    int startSrcTime;  // in MSEC
    int startDispTime; // in MSEC
    int duration; // in MSEC
    int stopSrcTime() {return startSrcTime + duration;}
    int stopDispTime() {return startDispTime + duration;}
    int shiftTime() {return startDispTime - startSrcTime;}
    int index;
    std::string title;
  };

  std::list<DemuxerInfo> m_demuxer;
  std::map<int,ChapterInfo> m_chapters;  // maps chapter end display time to chapter info

  DemuxerInfo *m_pCurDemuxInfo;
  ChapterInfo *m_pCurChapter;
  double m_lastDispPts;
};

// vim: ts=2 sw=2 expandtab
