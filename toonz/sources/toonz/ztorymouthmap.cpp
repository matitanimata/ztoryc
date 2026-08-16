#include "ztorymouthmap.h"

#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

//============================================================================

const char *const ZtoryMouthMap::kShapes[10] = {
    "ai", "e", "o", "u", "fv", "l", "mbp", "wq", "etc", "rest"};

int ZtoryMouthMap::shapeIndex(const QString &shape) {
  const QString s = shape.trimmed().toLower();
  // Il percorso Papagayo scrive «other» dove noi scriviamo «etc»: lo accetta
  // lipsyncpopup.cpp e lo accettiamo qui, o un .dat importato perderebbe una
  // casella su dieci senza dirlo.
  if (s == "other") return 8;
  for (int i = 0; i < 10; i++)
    if (s == QLatin1String(kShapes[i])) return i;
  return -1;
}

QStringList MouthSet::extraLevels() const {
  QStringList out;
  for (const QVector<MouthTarget> &v : mouths)
    for (const MouthTarget &t : v)
      if (!t.levelName.isEmpty() && !out.contains(t.levelName))
        out << t.levelName;
  return out;
}

int MouthMap::indexOfSet(const QString &n) const {
  for (int i = 0; i < sets.size(); i++)
    if (sets[i].name == n) return i;
  return -1;
}

//----------------------------------------------------------------------------

TFilePath ZtoryMouthMap::pathFor(const TFilePath &ownerPath) {
  if (ownerPath.isEmpty()) return TFilePath();
  // withType tiene il nome COMPLETO, gruppo del PSD incluso
  // (CH_giornalista#7#group.psd -> CH_giornalista#7#group.zmouth), che e'
  // esattamente cio' che tiene separate le mappe di livelli diversi dentro lo
  // stesso file fisico.
  return ownerPath.withType("zmouth");
}

bool ZtoryMouthMap::exists(const TFilePath &ownerPath, const QString &subScene) {
  const TFilePath p = pathFor(ownerPath);
  if (p.isEmpty()) return false;
  if (!QFile::exists(QString::fromStdWString(p.getWideString()))) return false;
  // Il file c'e', ma potrebbe contenere le mappe di ALTRE sotto-scene: dire di
  // si' senza guardare metterebbe il pallino accanto a una sotto-scena che non
  // e' mappata.
  MouthMap tmp;
  return load(ownerPath, subScene, tmp) && !tmp.sets.isEmpty();
}

//----------------------------------------------------------------------------

void ZtoryMouthMap::mappedSubScenes(const TFilePath &ownerPath,
                                    QSet<QString> *out) {
  if (!out) return;
  out->clear();
  const TFilePath p = pathFor(ownerPath);
  if (p.isEmpty()) return;
  const QString file = QString::fromStdWString(p.getWideString());
  if (!QFile::exists(file)) return;
  QFile f(file);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

  // Basta scorrere le intestazioni dei set: non servono ne' le caselle ne' i
  // bersagli, e leggerli sarebbe il grosso del lavoro.
  QXmlStreamReader xml(&f);
  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement()) continue;
    if (xml.name() != QLatin1String("mouthSet")) continue;
    const QXmlStreamAttributes a = xml.attributes();
    if (a.value("name").isEmpty()) continue;
    const QString sub = a.value("subscene").toString();
    if (!sub.isEmpty()) out->insert(sub);
  }
}

//----------------------------------------------------------------------------

bool ZtoryMouthMap::load(const TFilePath &ownerPath, const QString &subScene,
                         MouthMap &out, QString *error) {
  auto fail = [&](const QString &msg) {
    if (error) *error = msg;
    return false;
  };
  if (error) error->clear();

  const TFilePath p = pathFor(ownerPath);
  if (p.isEmpty()) return fail(QObject::tr("no level to read the mouths of"));
  const QString file = QString::fromStdWString(p.getWideString());
  if (!QFile::exists(file))
    return fail(QObject::tr("%1 has no mouth mapping")
                    .arg(QString::fromStdWString(ownerPath.getWideName())));

  QFile f(file);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return fail(QObject::tr("cannot read %1").arg(QFileInfo(file).fileName()));

  MouthMap map;
  MouthSet current;
  bool inSet   = false;
  int  slotIdx = -1;

  QXmlStreamReader xml(&f);
  while (!xml.atEnd()) {
    xml.readNext();

    if (xml.isStartElement()) {
      const QStringRef n           = xml.name();
      const QXmlStreamAttributes a = xml.attributes();

      if (n == QLatin1String("character")) {
        map.characterUuid = a.value("uuid").toString();
        map.characterName = a.value("name").toString();
      } else if (n == QLatin1String("mouthSet")) {
        current            = MouthSet();
        current.name       = a.value("name").toString();
        current.view       = a.value("view").toString();
        current.expression = a.value("expression").toString();
        current.variant    = a.value("variant").toString();
        current.subScene   = a.value("subscene").toString();
        inSet              = true;
      } else if (n == QLatin1String("slot") && inSet) {
        // Un viseme sconosciuto si SALTA invece di far fallire la lettura: un
        // file scritto da una versione futura con undici caselle deve restare
        // leggibile per le dieci che questa conosce. slotIdx = -1 fa cadere
        // anche i suoi <target>, che e' cio' che si vuole.
        slotIdx = shapeIndex(a.value("shape").toString());
      } else if (n == QLatin1String("target") && inSet && slotIdx >= 0) {
        MouthTarget t;
        t.levelName = a.value("level").toString();  // vuoto = livello ancora
        t.poseName  = a.value("pose").toString();
        if (a.hasAttribute("frame"))
          t.frameId = TFrameId(a.value("frame").toString().toInt(),
                               a.value("letter").toString());
        if (!t.isEmpty()) current.mouths[slotIdx].push_back(t);
      }
    } else if (xml.isEndElement()) {
      if (xml.name() == QLatin1String("slot")) slotIdx = -1;
      else if (xml.name() == QLatin1String("mouthSet")) {
        // Solo i set della sotto-scena chiesta (vuoto = quelli del livello).
        // Gli altri restano nel file e li rivede chi li ha scritti.
        if (inSet && !current.name.isEmpty() && current.subScene == subScene)
          map.sets.push_back(current);
        inSet   = false;
        slotIdx = -1;
      }
    }
  }

  if (xml.hasError())
    return fail(QObject::tr("%1 is damaged: %2")
                    .arg(QFileInfo(file).fileName(), xml.errorString()));

  out = map;
  return true;
}

//----------------------------------------------------------------------------

bool ZtoryMouthMap::save(const TFilePath &ownerPath, const QString &subScene,
                         const MouthMap &map, QString *error) {
  auto fail = [&](const QString &msg) {
    if (error) *error = msg;
    return false;
  };
  if (error) error->clear();

  const TFilePath p = pathFor(ownerPath);
  if (p.isEmpty()) return fail(QObject::tr("no level to write the mouths of"));
  const QString file = QString::fromStdWString(p.getWideString());

  // ⚠️ Il file di una SCENA tiene le mappe di piu' sotto-scene. Prima si
  // rileggono TUTTE, cosi' salvare i labiali di «testa» non cancella quelli di
  // «testa_profilo». Senza questo passaggio il secondo salvataggio butterebbe
  // via il primo, in silenzio.
  QVector<MouthSet> others;
  if (QFile::exists(file)) {
    QFile rf(file);
    if (rf.open(QIODevice::ReadOnly | QIODevice::Text)) {
      MouthSet cur;
      bool inSet   = false;
      int  slotIdx = -1;
      QXmlStreamReader rx(&rf);
      while (!rx.atEnd()) {
        rx.readNext();
        if (rx.isStartElement()) {
          const QXmlStreamAttributes a = rx.attributes();
          if (rx.name() == QLatin1String("mouthSet")) {
            cur             = MouthSet();
            cur.name        = a.value("name").toString();
            cur.view        = a.value("view").toString();
            cur.expression  = a.value("expression").toString();
            cur.variant     = a.value("variant").toString();
            cur.subScene    = a.value("subscene").toString();
            inSet           = true;
          } else if (rx.name() == QLatin1String("slot") && inSet) {
            slotIdx = shapeIndex(a.value("shape").toString());
          } else if (rx.name() == QLatin1String("target") && inSet &&
                     slotIdx >= 0) {
            MouthTarget t;
            t.levelName = a.value("level").toString();
            t.poseName  = a.value("pose").toString();
            if (a.hasAttribute("frame"))
              t.frameId = TFrameId(a.value("frame").toString().toInt(),
                                   a.value("letter").toString());
            if (!t.isEmpty()) cur.mouths[slotIdx].push_back(t);
          }
        } else if (rx.isEndElement()) {
          if (rx.name() == QLatin1String("slot")) slotIdx = -1;
          else if (rx.name() == QLatin1String("mouthSet")) {
            if (inSet && !cur.name.isEmpty() && cur.subScene != subScene)
              others.push_back(cur);
            inSet   = false;
            slotIdx = -1;
          }
        }
      }
    }
  }

  // Quali set valgono la pena di essere scritti. Si decide PRIMA di aprire il
  // file: se non ne resta nessuno il file va tolto, non svuotato.
  QVector<const MouthSet *> keep;
  for (const MouthSet &ms : others) keep.push_back(&ms);
  for (const MouthSet &ms : map.sets)
    if (!ms.name.isEmpty() && ms.isUsable()) keep.push_back(&ms);

  if (keep.isEmpty()) {
    // L'ESISTENZA del file dichiara «questo livello e' delle bocche». Lasciarne
    // uno vuoto direbbe una cosa falsa a chiunque lo controlli dopo.
    if (QFile::exists(file) && !QFile::remove(file))
      return fail(QObject::tr("cannot remove %1").arg(QFileInfo(file).fileName()));
    return true;
  }

  QFile f(file);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    return fail(QObject::tr("cannot write %1").arg(QFileInfo(file).fileName()));

  QXmlStreamWriter xml(&f);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztorymouths");
  xml.writeAttribute("version", "1");

  if (!map.characterUuid.isEmpty() || !map.characterName.isEmpty()) {
    xml.writeStartElement("character");
    if (!map.characterUuid.isEmpty())
      xml.writeAttribute("uuid", map.characterUuid);
    if (!map.characterName.isEmpty())
      xml.writeAttribute("name", map.characterName);
    xml.writeEndElement();
  }

  for (const MouthSet *ms : keep) {
    xml.writeStartElement("mouthSet");
    xml.writeAttribute("name", ms->name);
    if (!ms->view.isEmpty()) xml.writeAttribute("view", ms->view);
    if (!ms->expression.isEmpty())
      xml.writeAttribute("expression", ms->expression);
    if (!ms->variant.isEmpty()) xml.writeAttribute("variant", ms->variant);
    // Vuoto = il livello accanto a cui sta il file; altrimenti la sotto-scena.
    if (!ms->subScene.isEmpty()) xml.writeAttribute("subscene", ms->subScene);
    for (int i = 0; i < 10; i++) {
      const QVector<MouthTarget> &v = ms->mouths[i];
      // Una casella vuota si omette, non si finge. Anche una che contenesse
      // solo bersagli vuoti: scriverla direbbe «questo viseme e' mappato»
      // quando non lo e'.
      bool any = false;
      for (const MouthTarget &t : v) if (!t.isEmpty()) { any = true; break; }
      if (!any) continue;

      xml.writeStartElement("slot");
      xml.writeAttribute("shape", kShapes[i]);
      for (const MouthTarget &t : v) {
        if (t.isEmpty()) continue;
        xml.writeStartElement("target");
        // Il livello ancora NON si nomina: e' implicito, ed e' cio' che rende
        // il caso normale (un livello solo) impossibile da sbagliare.
        if (!t.levelName.isEmpty()) xml.writeAttribute("level", t.levelName);
        if (t.isPose()) {
          xml.writeAttribute("pose", t.poseName);
        } else {
          xml.writeAttribute("frame", QString::number(t.frameId.getNumber()));
          if (!t.frameId.getLetter().isEmpty())
            xml.writeAttribute("letter", t.frameId.getLetter());
        }
        xml.writeEndElement();
      }
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }

  xml.writeEndElement();  // ztorymouths
  xml.writeEndDocument();

  f.close();
  if (f.error() != QFile::NoError)
    return fail(QObject::tr("writing %1 failed").arg(QFileInfo(file).fileName()));
  return true;
}
