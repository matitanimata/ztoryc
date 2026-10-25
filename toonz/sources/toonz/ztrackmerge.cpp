#include "ztrackmerge.h"

#include <QHash>
#include <QPair>
#include <QVector>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

namespace {

struct Node {
  QString name;
  QVector<QPair<QString, QString>> attrs;
  QVector<Node> kids;
  QString key;    // identity among its siblings
  QString canon;  // canonical text: equal canon = equal content
};

// The attribute that identifies an element among its siblings. Elements not
// listed here are matched by name and position (project, team, assets, …).
QString keyAttrFor(const QString &name) {
  static const QHash<QString, QString> keys = {
      {"asset", "uuid"},      {"shot", "uuid"},         {"person", "name"},
      {"technique", "name"},  {"assetType", "name"},    {"storyboard", "file"},
      {"alias", "name"},      {"atask", "type"},        {"task", "type"},
      {"needs", "asset"},     {"castSynced", "asset"},  {"removed", "kitsuShotId"}};
  return keys.value(name);
}

const QString *attr(const Node &n, const QString &name) {
  for (const auto &a : n.attrs)
    if (a.first == name) return &a.second;
  return nullptr;
}

void finalize(Node &n) {
  // Keys of the children; a repeated key gets a suffix so nothing is lost.
  QHash<QString, int> seen;
  for (Node &k : n.kids) {
    const QString ka = keyAttrFor(k.name);
    const QString *v = ka.isEmpty() ? nullptr : attr(k, ka);
    QString key      = k.name + (v ? "=" + *v : QString());
    const int count  = seen.value(key, 0);
    seen[key]        = count + 1;
    if (!v || count > 0) key += "#" + QString::number(count);
    k.key = key;
  }
  QVector<QPair<QString, QString>> sorted = n.attrs;
  std::sort(sorted.begin(), sorted.end());
  QString c = "<" + n.name;
  for (const auto &a : sorted) c += " " + a.first + "=\"" + a.second + "\"";
  c += ">";
  for (const Node &k : n.kids) c += k.canon;
  c += "</" + n.name + ">";
  n.canon = c;
}

bool readNode(QXmlStreamReader &x, Node &n) {
  n.name = x.name().toString();
  for (const QXmlStreamAttribute &a : x.attributes())
    n.attrs << qMakePair(a.qualifiedName().toString(), a.value().toString());
  while (!x.atEnd()) {
    x.readNext();
    if (x.isStartElement()) {
      Node k;
      if (!readNode(x, k)) return false;
      n.kids << k;
    } else if (x.isEndElement()) {
      finalize(n);
      return true;
    }
  }
  return false;
}

bool parse(const QByteArray &bytes, Node &root) {
  QXmlStreamReader x(bytes);
  while (!x.atEnd()) {
    x.readNext();
    if (x.isStartElement()) {
      if (!readNode(x, root)) return false;
      while (!x.atEnd()) x.readNext();  // reach the end, to catch trailing junk
      return !x.hasError();
    }
  }
  return false;
}

void writeNode(QXmlStreamWriter &w, const Node &n) {
  w.writeStartElement(n.name);
  for (const auto &a : n.attrs) w.writeAttribute(a.first, a.second);
  for (const Node &k : n.kids) writeNode(w, k);
  w.writeEndElement();
}

QString describe(const QString &path, const Node &n) {
  return path + "/" + n.key;
}

Node mergeNode(const Node *b, const Node &o, const Node &t, const QString &path,
               QStringList *conflicts);

QVector<Node> mergeKids(const QVector<Node> &bk, const QVector<Node> &ok,
                        const QVector<Node> &tk, const QString &path,
                        QStringList *conflicts) {
  QHash<QString, const Node *> B, O, T;
  for (const Node &n : bk) B[n.key] = &n;
  for (const Node &n : ok) O[n.key] = &n;
  for (const Node &n : tk) T[n.key] = &n;

  QVector<Node> out;
  for (const Node &o : ok) {
    const Node *b = B.value(o.key), *t = T.value(o.key);
    if (t) {
      out << mergeNode(b, o, *t, path, conflicts);
    } else if (b) {
      // Deleted on their side: gone, unless we changed it meanwhile.
      if (o.canon != b->canon) {
        out << o;
        if (conflicts)
          *conflicts << describe(path, o) + ": deleted elsewhere, changed here — kept";
      }
    } else {
      out << o;  // added here
    }
  }
  // What only they have, placed after the item that precedes it on their side.
  for (int i = 0; i < tk.size(); ++i) {
    const Node &t = tk[i];
    if (O.contains(t.key)) continue;
    const Node *b = B.value(t.key);
    if (b && t.canon == b->canon) continue;  // deleted here, untouched there
    if (b && conflicts)
      *conflicts << describe(path, t) + ": deleted here, changed elsewhere — kept";
    int at = 0;
    for (int j = i - 1; j >= 0 && at == 0; --j)
      for (int r = 0; r < out.size(); ++r)
        if (out[r].key == tk[j].key) {
          at = r + 1;
          break;
        }
    out.insert(at, t);
  }
  return out;
}

Node mergeNode(const Node *b, const Node &o, const Node &t, const QString &path,
               QStringList *conflicts) {
  if (o.canon == t.canon) return o;
  if (b && o.canon == b->canon) return t;
  if (b && t.canon == b->canon) return o;

  // Changed on both sides: attribute by attribute, then the children.
  Node r;
  r.name = o.name;
  QStringList names;
  for (const auto &a : o.attrs) names << a.first;
  for (const auto &a : t.attrs)
    if (!names.contains(a.first)) names << a.first;
  for (const QString &name : names) {
    const QString *ov = attr(o, name), *tv = attr(t, name);
    const QString *bv = b ? attr(*b, name) : nullptr;
    auto same = [](const QString *x, const QString *y) {
      return (!x && !y) || (x && y && *x == *y);
    };
    const QString *v;
    if (same(ov, tv) || same(bv, tv))
      v = ov;
    else if (same(bv, ov))
      v = tv;
    else {
      v = ov;
      if (conflicts)
        *conflicts << describe(path, o) + "@" + name +
                          ": changed on both sides — this window's value kept";
    }
    if (v) r.attrs << qMakePair(name, *v);
  }
  r.kids = mergeKids(b ? b->kids : QVector<Node>(), o.kids, t.kids,
                     describe(path, o), conflicts);
  r.key = o.key;
  finalize(r);
  r.key = o.key;  // finalize() sets the children's keys, not its own
  return r;
}

}  // namespace

namespace ZtrackMerge {

QByteArray merge(const QByteArray &base, const QByteArray &ours,
                 const QByteArray &theirs, QStringList *conflicts, bool *ok) {
  Node b, o, t;
  const bool parsed = parse(base, b) && parse(ours, o) && parse(theirs, t) &&
                      o.name == t.name;
  if (ok) *ok = parsed;
  if (!parsed) return QByteArray();
  const Node r = mergeNode(b.name == o.name ? &b : nullptr, o, t, QString(),
                           conflicts);
  QByteArray out;
  QXmlStreamWriter w(&out);
  w.setAutoFormatting(true);
  w.writeStartDocument();
  writeNode(w, r);
  w.writeEndDocument();
  return out;
}

bool sameContent(const QByteArray &a, const QByteArray &b) {
  Node x, y;
  return parse(a, x) && parse(b, y) && x.canon == y.canon;
}

bool isWellFormed(const QByteArray &xml) {
  Node n;
  return parse(xml, n);
}

}  // namespace ZtrackMerge
