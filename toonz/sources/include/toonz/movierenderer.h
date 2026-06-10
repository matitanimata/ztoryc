#pragma once

#ifndef MOVIERENDERER_INCLUDED
#define MOVIERENDERER_INCLUDED

#include <QObject>
#include "trenderer.h"

#include <string>
#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================================================

//  Forward declarations

class TFilePath;
class TRenderer;
class ToonzScene;

//=================================================================================

//=========================================================
//
//    MovieRenderer
//
//---------------------------------------------------------

/*!
MovieRenderer is the high-level API class responsible for rendering
Toonz scenes into movies.
In a more generic view, the term 'movie' represents here a generic sequence
of images, which may even be kept in memory rather than written to file.
*/

class DVAPI MovieRenderer final : public QObject {
  Q_OBJECT

  class Imp;
  Imp *m_imp;

public:
  class Listener {
  public:
    virtual bool onFrameCompleted(int frame) = 0;
    virtual bool onFrameFailed(int frame, TException &e) = 0;
    virtual void onSequenceCompleted(const TFilePath &fp) = 0;
    virtual ~Listener() {}
  };

public:
  MovieRenderer(ToonzScene *scene, const TFilePath &moviePath,
                int threadCount = 1, bool cacheResults = true);

  ~MovieRenderer();

  void setRenderSettings(const TRenderSettings &renderData);
  void setDpi(double xDpi, double yDpi);

  // Fix for sequential batch renders: capture the audio range at setup time
  // instead of re-reading it from TOutputProperties at render completion, which
  // would pick up a different shot's range if properties were updated meanwhile.
  void setAudioRange(int r0, int r1);

  // Ztoryc animatic burn-in: overlay timecode and/or shot-name labels on every
  // saved frame.  Pinned at setup time (same rationale as setAudioRange).
  // Segments map inclusive scene-frame ranges to labels (e.g. SQ010_SH020_P001).
  struct BurnInSegment {
    int from, to;
    std::wstring label;
  };
  void setBurnIn(bool timecode, double fps,
                 const std::vector<BurnInSegment> &segments);

  void addListener(Listener *listener);

  void enablePrecomputing(bool on);
  bool isPrecomputingEnabled() const;

  TRenderer *getTRenderer();

  void addFrame(double frame, const TFxPair &fx);

  void start();

public slots:

  void onCanceled();

private:
  // not implemented
  MovieRenderer(const MovieRenderer &);
  MovieRenderer &operator=(const MovieRenderer &);
};

#endif
