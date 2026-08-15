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

class ZtoryDialogueHighlighter final : public QSyntaxHighlighter {
public:
  explicit ZtoryDialogueHighlighter(QTextDocument *doc)
      : QSyntaxHighlighter(doc) {}

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
    if (!ZtoryDialogueHighlighter::enabled()) return;
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
};
