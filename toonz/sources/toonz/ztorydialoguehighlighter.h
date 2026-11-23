#pragma once

//============================================================================
// ZtoryDialogueHighlighter — colora il nome del personaggio dentro il campo
// dialogo, dove il nome sta.
//
// Idea di Franco (2026-08-15): «non potrebbe bastare evidenziare in verde il
// nome del personaggio, in modo che si capisca che è stato riconosciuto?».
// Sì, ed è meglio: il riscontro va dove sta la causa, non in una riga a parte
// che fa ballare l'altezza del pannello a ogni ridisegno.
//
// Resta utile una riga di avviso SOLO per i nomi non riconosciuti: quelli sono
// l'unico caso su cui si deve agire, e in un campo lungo e scrollato
// resterebbero fuori vista.
//
// Niente Q_OBJECT di proposito: non servono segnali, e così l'header sta da
// solo senza toccare il CMakeLists per il moc.
//
// La REGOLA di cosa sia un'intestazione non vive qui: è
// ZtoryModel::speakerAt(), la stessa che usa il parser. Due copie divergono al
// primo caso limite, ed è esattamente l'errore che i test hanno già preso una
// volta in questa feature.
//============================================================================

#include "ztorymodel.h"
#include "toonz/preferences.h"

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QColor>
#include <QSet>
#include <QSignalBlocker>
#include <QCoreApplication>
#include <QTimer>

#include <functional>

// Il colore di una parola segnata, per tipo di asset (spoglio a parole,
// 2026-11-22). Sottolineata, e non in grassetto: il grassetto verde resta il
// segno dei personaggi.
inline QColor ztoryWordTagColor(const QString &type) {
  if (type.compare("Prop", Qt::CaseInsensitive) == 0) return QColor("#4FC3F7");
  if (type.compare("Environment", Qt::CaseInsensitive) == 0 ||
      type.compare("BG", Qt::CaseInsensitive) == 0)
    return QColor("#C792EA");
  if (type.compare("FX", Qt::CaseInsensitive) == 0) return QColor("#F06292");
  if (ZtoryModel::isCharacterType(type)) return QColor("#22D160");
  return QColor("#FFD54F");
}

class ZtoryDialogueHighlighter final : public QSyntaxHighlighter {
public:
  // speakers = false for the Action field: there a line in capitals is a
  // description, not who speaks; only the tagged words are coloured.
  explicit ZtoryDialogueHighlighter(QTextDocument *doc, bool speakers = true)
      : QSyntaxHighlighter(doc), m_speakers(speakers) {
    all().insert(this);
    hookModelOnce();
  }
  ~ZtoryDialogueHighlighter() override { all().remove(this); }

  // The shot the field shows, for the words «only mentioned» in it (set by
  // ZtoryWordTagWatcher, which knows it). Without it every tag looks active.
  void setShotUuidProvider(std::function<QString()> p) { m_shotUuid = std::move(p); }

  // A tag or an alias changes the colours of EVERY field, not only the one
  // where it was given: the same word may be written in forty panels.
  static void rehighlightAll() {
    for (ZtoryDialogueHighlighter *h : all()) {
      // Ricolorare fa emettere al documento contentsChanged, cioe' il
      // textChanged del campo: nel Board ogni pannello si dichiarerebbe
      // modificato e rimanderebbe il suo testo al modello. Il colore non e'
      // una modifica, e il disegno passa dal layout, che non e' bloccato.
      QSignalBlocker block(h->document());
      h->rehighlight();
    }
  }

  // Spento dalla preferenza, oppure quando il progetto non ha NESSUN
  // personaggio: li' ogni riga in maiuscolo diventerebbe un avviso arancione,
  // cioe' rumore invece di informazione. Chi non usa i personaggi non deve
  // accorgersi che questa cosa esiste.
  static bool enabled() {
    if (!Preferences::instance()->isDialogueSpeakerHighlight()) return false;
    for (const Asset &a : ZtoryModel::instance()->assets())
      if (a.type.compare("Character", Qt::CaseInsensitive) == 0) return true;
    return false;
  }

protected:
  void highlightBlock(const QString &text) override {
    if (text.trimmed().isEmpty()) return;
    // Le parole segnate non dipendono da enabled(): quello e' falso in un
    // progetto senza personaggi, dove lo spoglio degli oggetti serve lo stesso.
    // Solo nei campi senza personaggi (Action, Notes): nel Dialog lo spoglio
    // a parole non vale (Franco, 2026-11-23), e colorarle li' direbbe il
    // contrario.
    if (!m_speakers) highlightWordTags(text);
    if (!m_speakers || !ZtoryDialogueHighlighter::enabled()) return;
    // La riga dopo serve alla regola di Fountain (nome seguito da qualcosa).
    const QTextBlock next = currentBlock().next();
    const QString nextText = next.isValid() ? next.text() : QString();

    QString name;
    bool matched = false;
    if (!ZtoryModel::instance()->speakerAt(text, nextText, &name, &matched))
      return;

    QTextCharFormat fmt;
    // Verde = personaggio del progetto. Arancione = nome che il testo dichiara
    // ma il progetto non conosce: quasi sempre uno script incollato con un
    // personaggio non ancora creato.
    fmt.setForeground(matched ? QColor("#22D160") : QColor("#F5A623"));
    fmt.setFontWeight(QFont::Bold);

    // Si colora il NOME, non tutta la riga: nella forma «MARIO: ma dove vai?»
    // la battuta resta di colore normale, e si vede dove finisce il nome.
    const int start = text.indexOf(name);
    if (start < 0) return;
    setFormat(start, name.length(), fmt);
  }

private:
  void highlightWordTags(const QString &text) {
    ZtoryModel *m = ZtoryModel::instance();
    const QVector<ZtoryWordTagHit> hits = m->findWordTags(text);
    if (hits.isEmpty()) return;
    const QString shot = m_shotUuid ? m_shotUuid() : QString();
    for (const ZtoryWordTagHit &h : hits) {
      QTextCharFormat fmt;
      if (!shot.isEmpty() && m->wordTagSkipped(shot, h.key)) {
        // Solo nominata in questo shot: grigia e tratteggiata, cosi' si vede
        // che il tag c'e' ma qui non porta l'asset nel Breakdown.
        fmt.setForeground(QColor("#9E9E9E"));
        fmt.setUnderlineStyle(QTextCharFormat::DashUnderline);
      } else {
        QString type;
        for (const Asset &a : m->assets())
          if (a.uuid == h.assetUuid) { type = a.type; break; }
        fmt.setForeground(ztoryWordTagColor(type));
        fmt.setFontUnderline(true);
      }
      setFormat(h.pos, h.len, fmt);
    }
  }

  // Il testo dei pannelli arriva PRIMA dei personaggi: il Board riempie i campi
  // e solo dopo carica il tracker. Colorati in quel momento, i nomi restavano
  // bianchi finche' non si toccava il testo (Franco, 2026-11-22, Cascina).
  // Quindi si ricolora tutto quando il progetto e' caricato e quando cambiano
  // gli asset — una volta sola per raffica: assetsChanged arriva anche a ogni
  // stato di task cambiato.
  static void hookModelOnce() {
    static bool hooked = false;
    if (hooked) return;
    hooked = true;
    QTimer *t = new QTimer(QCoreApplication::instance());
    t->setSingleShot(true);
    t->setInterval(0);
    QObject::connect(t, &QTimer::timeout, [] { rehighlightAll(); });
    ZtoryModel *m = ZtoryModel::instance();
    QObject::connect(m, &ZtoryModel::productionReloaded, t,
                     QOverload<>::of(&QTimer::start));
    QObject::connect(m, &ZtoryModel::assetsChanged, t,
                     QOverload<>::of(&QTimer::start));
  }

  static QSet<ZtoryDialogueHighlighter *> &all() {
    static QSet<ZtoryDialogueHighlighter *> s;
    return s;
  }

  bool m_speakers = true;
  std::function<QString()> m_shotUuid;
};
