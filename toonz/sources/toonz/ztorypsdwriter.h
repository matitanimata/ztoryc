#pragma once

// Ztoryc: a layered PSD writer — layers and groups (Franco, 2026-10-27).
//
// Written for Export Layout: one PSD per shot, a group per column with one
// layer per drawing, so that importing it back with «Expose layers in a group
// as frames in a column» gives the same columns. The structure is the one
// Puppetoonz writes and Ztoryc's PSD reader is known to read: a group is a
// closing «section divider» record, its layers, then the folder record, in
// bottom-to-top order; names also as «luni» (Unicode).
// 8 bits per channel, RGBA, PackBits; each layer cropped to its pixels.

#include <QImage>
#include <QString>
#include <QVector>

struct ZtoryPsdLayer {
  QString name;
  QImage image;   // canvas-sized; transparent outside the drawing
  bool visible = true;
};

struct ZtoryPsdGroup {
  QString name;                   // empty: the layers stand alone, no group
  QVector<ZtoryPsdLayer> layers;  // top to bottom
};

// `groups` top to bottom, as in Photoshop's Layers panel. `merged` is the
// flattened picture stored for readers that do not read layers.
bool ztoryWritePsd(const QString &path, int width, int height,
                   const QVector<ZtoryPsdGroup> &groups, const QImage &merged,
                   QString *why = nullptr);
