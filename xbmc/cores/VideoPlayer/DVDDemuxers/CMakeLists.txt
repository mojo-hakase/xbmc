set(SOURCES DemuxMultiSource.cpp
            DemuxTimeline.cpp
            DVDDemux.cpp
            DVDDemuxBXA.cpp
            DVDDemuxCC.cpp
            DVDDemuxCDDA.cpp
            DVDDemuxClient.cpp
            DVDDemuxFFmpeg.cpp
            DVDDemuxUtils.cpp
            DVDDemuxVobsub.cpp
            DVDFactoryDemuxer.cpp)

set(HEADERS DemuxMultiSource.h
            DemuxTimeline.h
            DVDDemux.h
            DVDDemuxBXA.h
            DVDDemuxCC.h
            DVDDemuxCDDA.h
            DVDDemuxClient.h
            DVDDemuxFFmpeg.h
            DVDDemuxPacket.h
            DVDDemuxUtils.h
            DVDDemuxVobsub.h
            DVDFactoryDemuxer.h)

core_add_library(dvddemuxers)
