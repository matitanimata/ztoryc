#include "ztorypsdwriter.h"

#include <QByteArray>
#include <QFile>
#include <QRect>
#include <QtEndian>

namespace {

void put16(QByteArray &b, quint16 v) {
  char c[2];
  qToBigEndian(v, c);
  b.append(c, 2);
}
void put32(QByteArray &b, quint32 v) {
  char c[4];
  qToBigEndian(v, c);
  b.append(c, 4);
}

// A Pascal name padded to a multiple of 4, as layer records want it.
QByteArray pascal4(const QString &name) {
  QByteArray n = name.toLatin1().left(255);
  QByteArray b;
  b.append(char(n.size()));
  b.append(n);
  while (b.size() % 4) b.append('\0');
  return b;
}

// The «luni» block: the name in UTF-16, for names Latin-1 cannot hold.
QByteArray luni(const QString &name) {
  QByteArray d;
  put32(d, name.size());
  for (QChar ch : name) put16(d, ch.unicode());
  if (d.size() % 2) d.append('\0');
  QByteArray b("8BIMluni");
  put32(b, d.size());
  b.append(d);
  return b;
}

// PackBits, the run-length compression PSD calls 1: one row at a time.
QByteArray packRow(const char *src, int n) {
  QByteArray out;
  int i = 0;
  while (i < n) {
    int run = 1;
    while (i + run < n && run < 128 && src[i + run] == src[i]) run++;
    if (run > 1) {
      out.append(char(1 - run));
      out.append(src[i]);
      i += run;
      continue;
    }
    int lit = 1;
    while (i + lit < n && lit < 128 &&
           !(i + lit + 1 < n && src[i + lit] == src[i + lit + 1]))
      lit++;
    out.append(char(lit - 1));
    out.append(src + i, lit);
    i += lit;
  }
  return out;
}

// A whole channel, compressed: the byte count of every row, then the rows.
QByteArray packChannel(const QByteArray &raw, int width, int height) {
  QByteArray counts, rows;
  for (int y = 0; y < height; y++) {
    const QByteArray r = packRow(raw.constData() + y * width, width);
    put16(counts, quint16(r.size()));
    rows.append(r);
  }
  return counts + rows;
}

struct Record {
  QString name;
  QRect rect;            // in canvas pixels; empty for a section divider
  QByteArray ch[4];      // alpha, R, G, B — raw, cropped to rect
  int section = 0;       // lsct: 1 folder, 3 closing divider
  bool visible = true;
};

Record leaf(const ZtoryPsdLayer &l) {
  Record r;
  r.name    = l.name;
  r.visible = l.visible;
  const QImage img = l.image.convertToFormat(QImage::Format_ARGB32);
  // crop to the drawn pixels: a layer is mostly empty canvas
  int x0 = img.width(), y0 = img.height(), x1 = -1, y1 = -1;
  for (int y = 0; y < img.height(); y++) {
    const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
    for (int x = 0; x < img.width(); x++)
      if (qAlpha(row[x])) {
        x0 = qMin(x0, x); x1 = qMax(x1, x);
        y0 = qMin(y0, y); y1 = qMax(y1, y);
      }
  }
  if (x1 < 0) return r;  // empty: no pixels, the record still keeps its name
  r.rect = QRect(QPoint(x0, y0), QPoint(x1, y1));
  for (int y = y0; y <= y1; y++) {
    const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
    for (int x = x0; x <= x1; x++) {
      r.ch[0].append(char(qAlpha(row[x])));
      r.ch[1].append(char(qRed(row[x])));
      r.ch[2].append(char(qGreen(row[x])));
      r.ch[3].append(char(qBlue(row[x])));
    }
  }
  return r;
}

Record divider(const QString &name, int section) {
  Record r;
  r.name    = name;
  r.section = section;
  return r;
}

}  // namespace

bool ztoryWritePsd(const QString &path, int width, int height,
                   const QVector<ZtoryPsdGroup> &groups, const QImage &merged,
                   QString *why) {
  // Records go bottom to top. Inside a group the layers keep their order —
  // Ztoryc numbers a group's frames in record order, so the first frame is
  // the first record (Puppetoonz, 2026-08-27).
  QVector<Record> recs;
  for (int g = groups.size() - 1; g >= 0; g--) {
    const ZtoryPsdGroup &grp = groups[g];
    if (grp.name.isEmpty()) {
      for (int i = grp.layers.size() - 1; i >= 0; i--) recs << leaf(grp.layers[i]);
      continue;
    }
    recs << divider("</Layer group>", 3);  // the name Ztoryc looks for
    for (const ZtoryPsdLayer &l : grp.layers) recs << leaf(l);
    recs << divider(grp.name, 1);
  }

  QByteArray records, chandata;
  static const qint16 ids[4] = {-1, 0, 1, 2};
  for (const Record &r : recs) {
    const bool empty = r.rect.isEmpty();
    put32(records, empty ? 0 : r.rect.top());
    put32(records, empty ? 0 : r.rect.left());
    put32(records, empty ? 0 : r.rect.bottom() + 1);
    put32(records, empty ? 0 : r.rect.right() + 1);
    put16(records, 4);  // four channels even when empty: Krita wants them
    QByteArray packed[4];
    for (int c = 0; c < 4; c++) {
      packed[c] = empty ? QByteArray()
                        : packChannel(r.ch[c], r.rect.width(), r.rect.height());
      put16(records, quint16(ids[c]));
      put32(records, 2 + packed[c].size());  // the compression field + data
    }
    records.append("8BIM");
    records.append(r.section == 1 ? "pass" : "norm");
    records.append(char(255));                 // opacity
    records.append(char(0));                   // clipping
    records.append(char(r.visible ? 0 : 2));   // bit 1 = hidden
    records.append(char(0));                   // filler
    QByteArray extra;
    put32(extra, 0);  // no mask
    put32(extra, 0);  // no blending ranges
    extra.append(pascal4(r.name));
    extra.append(luni(r.name));
    if (r.section) {
      extra.append("8BIMlsct");
      put32(extra, 4);
      put32(extra, r.section);
    }
    if (extra.size() % 2) extra.append('\0');
    put32(records, extra.size());
    records.append(extra);
    for (int c = 0; c < 4; c++) {
      put16(chandata, empty ? 0 : 1);  // PackBits; raw for an empty divider
      chandata.append(packed[c]);
    }
  }

  QByteArray info;
  put16(info, quint16(recs.size()));
  info.append(records);
  info.append(chandata);
  if (info.size() % 2) info.append('\0');
  QByteArray lam;
  put32(lam, info.size());
  lam.append(info);
  put32(lam, 0);  // no global layer mask

  // The flattened picture, on white: three raw planes.
  QImage flat(width, height, QImage::Format_ARGB32);
  flat.fill(Qt::white);
  if (!merged.isNull()) {
    QImage m = merged.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < qMin(height, m.height()); y++) {
      const QRgb *src = reinterpret_cast<const QRgb *>(m.constScanLine(y));
      QRgb *dst       = reinterpret_cast<QRgb *>(flat.scanLine(y));
      for (int x = 0; x < qMin(width, m.width()); x++) {
        const int a = qAlpha(src[x]);
        auto mix = [a](int c) { return (c * a + 255 * (255 - a)) / 255; };
        dst[x] = qRgb(mix(qRed(src[x])), mix(qGreen(src[x])), mix(qBlue(src[x])));
      }
    }
  }
  // three PackBits planes: all the row counts first, then all the rows
  QByteArray counts, rows;
  for (int c = 0; c < 3; c++)
    for (int y = 0; y < height; y++) {
      const QRgb *row = reinterpret_cast<const QRgb *>(flat.constScanLine(y));
      QByteArray raw(width, '\0');
      for (int x = 0; x < width; x++)
        raw[x] = char(c == 0 ? qRed(row[x]) : c == 1 ? qGreen(row[x]) : qBlue(row[x]));
      const QByteArray r = packRow(raw.constData(), width);
      put16(counts, quint16(r.size()));
      rows.append(r);
    }
  QByteArray planes;
  put16(planes, 1);
  planes.append(counts);
  planes.append(rows);

  QByteArray out("8BPS");
  put16(out, 1);
  out.append(QByteArray(6, '\0'));
  put16(out, 3);  // the merged image: RGB
  put32(out, height);
  put32(out, width);
  put16(out, 8);
  put16(out, 3);  // RGB colour mode
  put32(out, 0);  // no colour mode data
  put32(out, 0);  // no image resources
  put32(out, lam.size());  // the layer and mask section, then its content
  out.append(lam);
  out.append(planes);

  QFile f(path);
  if (!f.open(QIODevice::WriteOnly) || f.write(out) != out.size()) {
    if (why) *why = f.errorString();
    return false;
  }
  return true;
}
