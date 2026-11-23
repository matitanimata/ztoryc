#pragma once

//============================================================================
// Spoglio a parole — una parola del Dialog o dell'Action segnata come asset
// (Franco, 2026-11-22: «selezionando una parola la potessimo indicare come
// prop o bkg»).
//
// Le due decisioni di Franco:
//  - il tag vale per TUTTO il progetto, come gli alias dei personaggi: dove
//    «sedia» e' scritta, lo shot ha SEDIA nel Breakdown, senza rifarlo;
//  - se la parola sparisce dal testo, si CHIEDE se togliere l'asset anche dal
//    Breakdown: non si toglie da solo.
//
// Quando si confronta: all'uscita dal campo, non a ogni tasto. A ogni tasto,
// scrivendo «sediamo» dopo «sedia» l'asset entrerebbe e subito dopo
// comparirebbe la domanda a meta' parola.
// Cosa si confronta: gli asset segnati in TUTTI i pannelli dello shot, prima e
// dopo. Cosi' non si chiede niente se la parola resta in un altro pannello, e
// non si tocca cio' che nel Breakdown e' entrato a mano o da Kitsu.
//
// Un'intestazione sola per il Board e per l'Animatic: il menu degli alias e'
// gia' scritto due volte, e due copie si separano al primo ritocco.
// Niente Q_OBJECT, come il completamento dei nomi: niente moc da toccare.
//============================================================================

#include "ztorymodel.h"
#include "ztorydialoguehighlighter.h"

#include <QApplication>
#include <QFocusEvent>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QTextEdit>
#include <QTimer>

#include <functional>

// What changed in one shot while a field was being edited: tagged words that
// appeared go into the Breakdown, those that disappeared are asked about.
inline void ztoryReconcileWordTags(QWidget *parent, const QString &shotUuid,
                                   const QSet<QString> &before) {
  ZtoryModel *m      = ZtoryModel::instance();
  const ShotData *sd = m->shotByUuid(shotUuid);
  if (!sd || m->projectShotIndex(shotUuid) < 0) return;
  const QSet<QString> after = m->taggedAssetsInShot(*sd);
  const QString shotLabel   = sd->label();  // sd may not survive the dialog

  bool changed = false;
  for (const QString &u : after - before)
    changed |= m->addToShotBreakdown(shotUuid, u);

  QStringList gone, names;
  for (const QString &u : before - after) {
    if (!m->shotBreakdownHas(shotUuid, u)) continue;
    for (const Asset &a : m->assets())
      if (a.uuid == u) {
        gone << u;
        names << a.name;
        break;
      }
  }
  if (!gone.isEmpty()) {
    const QString msg =
        names.size() == 1
            ? QObject::tr("«%1» is no longer written in %2.\n\n"
                          "Remove it from the shot's breakdown too?")
                  .arg(names.first(), shotLabel)
            : QObject::tr("%1 are no longer written in %2.\n\n"
                          "Remove them from the shot's breakdown too?")
                  .arg(names.join(", "), shotLabel);
    if (QMessageBox::question(parent, QObject::tr("Breakdown"), msg,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) == QMessageBox::Yes) {
      for (const QString &u : gone) m->removeFromShotBreakdown(shotUuid, u);
      changed = true;
    }
  }
  if (changed) m->saveProjectDb();
}

class ZtoryWordTagWatcher final : public QObject {
public:
  // shotUuid: the shot the field is showing NOW (the Board reuses its panels).
  ZtoryWordTagWatcher(QTextEdit *field, std::function<QString()> shotUuid)
      : QObject(field), m_field(field), m_shotUuid(std::move(shotUuid)) {
    field->installEventFilter(this);
    // The highlighter greys the words «only mentioned» in this shot: it needs
    // to know the shot too, and only the watcher does.
    for (QObject *c : field->document()->children())
      if (auto *h = dynamic_cast<ZtoryDialogueHighlighter *>(c))
        h->setShotUuidProvider(m_shotUuid);
  }
  QString shotUuid() const { return m_shotUuid ? m_shotUuid() : QString(); }

protected:
  bool eventFilter(QObject *obj, QEvent *e) override {
    if (obj != m_field) return QObject::eventFilter(obj, e);
    if (e->type() == QEvent::FocusIn) {
      // Back from a menu or the names list: the edit is still the same one.
      const bool fromPopup =
          static_cast<QFocusEvent *>(e)->reason() == Qt::PopupFocusReason;
      if (!(fromPopup && !m_shot.isEmpty())) {
        m_shot = m_shotUuid();
        const ShotData *sd = ZtoryModel::instance()->shotByUuid(m_shot);
        m_before = sd ? ZtoryModel::instance()->taggedAssetsInShot(*sd)
                      : QSet<QString>();
      }
    } else if (e->type() == QEvent::FocusOut &&
               static_cast<QFocusEvent *>(e)->reason() !=
                   Qt::PopupFocusReason &&
               !m_shot.isEmpty()) {
      const QString shot          = m_shot;
      const QSet<QString> before  = m_before;
      QPointer<QWidget> parent(m_field->window());
      m_shot.clear();
      // After the focus change, not inside it: a dialog opened in the middle
      // of a focus event leaves the focus nowhere.
      QTimer::singleShot(0, qApp, [parent, shot, before]() {
        ztoryReconcileWordTags(parent, shot, before);
      });
    }
    return QObject::eventFilter(obj, e);
  }

private:
  QTextEdit *m_field;
  std::function<QString()> m_shotUuid;
  QString m_shot;
  QSet<QString> m_before;
};

// The shot a field is showing, asked to its watcher.
inline QString ztoryFieldShotUuid(QTextEdit *field) {
  for (QObject *c : field->children())
    if (auto *w = dynamic_cast<ZtoryWordTagWatcher *>(c)) return w->shotUuid();
  return QString();
}

// Tag `phrase` -> asset, then give the asset to every shot of the open
// storyboard where the phrase is written. A shot not in the tracker yet gets
// it when it is published (publishShotsToProjectDb).
inline void ztoryApplyWordTag(const QString &phrase, const QString &assetUuid) {
  ZtoryModel *m = ZtoryModel::instance();
  m->setWordTag(phrase, assetUuid);
  if (!assetUuid.isEmpty())
    for (const ShotData &sd : m->shots())
      if (m->taggedAssetsInShot(sd).contains(assetUuid))
        m->addToShotBreakdown(sd.uuid, assetUuid);
  m->saveProjectDb();
  ZtoryDialogueHighlighter::rehighlightAll();
}

// The asset's name in a menu, with ✓ when it is already in the breakdown of
// the field's shot (Franco, 2026-11-23: «come faccio a sapere se e' gia'
// inserito o se lo devo ancora taggare?»). The answer goes where the choice
// is made.
inline QString ztoryAssetMenuText(const Asset &a, const QString &shotUuid) {
  return !shotUuid.isEmpty() &&
                 ZtoryModel::instance()->shotBreakdownHas(shotUuid, a.uuid)
             ? a.name + QString::fromUtf8("  \u2713")
             : a.name;
}

// «Tag "sedia" as ▸ Prop ▸ SEDIA / New Prop…». Anche i personaggi (Franco,
// 2026-11-23): uno che non parla ma e' scritto nell'Action e' in scena lo
// stesso. Il menu si offre solo in Action e Notes; nel Dialog chi parla si
// riconosce dal testo, e «Treat as character» resta l'alias di chi parla.
inline void ztoryAddWordTagMenu(QMenu *menu, const QString &selection,
                                QTextEdit *field) {
  QWidget *parent = field->window();
  const QString phrase = selection.simplified();
  if (phrase.isEmpty() || phrase.length() > 40) return;
  ZtoryModel *m          = ZtoryModel::instance();
  const QString current  = m->wordTag(phrase);
  QMenu *tagMenu =
      menu->addMenu(QObject::tr("Tag «%1» as").arg(phrase));

  QStringList types;
  for (const AssetType &t : m->assetTypes())
    if (!types.contains(t.name, Qt::CaseInsensitive)) types << t.name;
  for (const Asset &a : m->assets())  // a type only an asset has (from Kitsu)
    if (!a.type.isEmpty() && !types.contains(a.type, Qt::CaseInsensitive))
      types << a.type;

  const QString fieldShot = ztoryFieldShotUuid(field);
  for (const QString &type : types) {
    QMenu *sub = tagMenu->addMenu(type);
    for (const Asset &a : m->assets()) {
      if (a.type.compare(type, Qt::CaseInsensitive) != 0) continue;
      QAction *act = sub->addAction(ztoryAssetMenuText(a, fieldShot));
      act->setCheckable(true);
      act->setChecked(current == a.uuid);
      const QString uuid = a.uuid;
      QObject::connect(act, &QAction::triggered,
                       [phrase, uuid]() { ztoryApplyWordTag(phrase, uuid); });
    }
    if (!sub->isEmpty()) sub->addSeparator();
    QAction *newAct = sub->addAction(
        QObject::tr("New %1 «%2»…").arg(type, phrase.toUpper()));
    QPointer<QWidget> par(parent);
    QObject::connect(newAct, &QAction::triggered, [phrase, type, par]() {
      bool ok = false;
      const QString name =
          QInputDialog::getText(par, QObject::tr("New %1").arg(type),
                                QObject::tr("Asset name:"), QLineEdit::Normal,
                                phrase.toUpper(), &ok)
              .trimmed();
      if (!ok || name.isEmpty()) return;
      ZtoryModel *m = ZtoryModel::instance();
      // Same name and type already there: that one, not a twin.
      QString uuid;
      for (const Asset &a : m->assets())
        if (a.type.compare(type, Qt::CaseInsensitive) == 0 &&
            ZtoryModel::assetNameKey(a.name) == ZtoryModel::assetNameKey(name))
          uuid = a.uuid;
      if (uuid.isEmpty()) {
        m->addAsset(type, name);
        uuid = m->assets().back().uuid;
      }
      ztoryApplyWordTag(phrase, uuid);
    });
  }
  if (!current.isEmpty()) {
    tagMenu->addSeparator();
    // Solo nominata QUI (Franco, 2026-11-22: la racchetta di cui si parla ma
    // che non si vede). Il tag resta negli altri shot; in questo l'asset esce
    // dal Breakdown subito — e' la cosa che si e' appena chiesta, non serve
    // un'altra domanda — a meno che un'altra parola dello shot lo porti.
    const QString shot = ztoryFieldShotUuid(field);
    if (!shot.isEmpty()) {
      ZtoryModel *m = ZtoryModel::instance();
      QAction *skip =
          tagMenu->addAction(QObject::tr("Not in this shot (only mentioned)"));
      skip->setCheckable(true);
      skip->setChecked(m->wordTagSkipped(shot, phrase));
      QObject::connect(skip, &QAction::toggled, [phrase, shot, current](bool on) {
        ZtoryModel *m = ZtoryModel::instance();
        m->setWordTagSkipped(shot, phrase, on);
        if (const ShotData *sd = m->shotByUuid(shot)) {
          const bool needed = m->taggedAssetsInShot(*sd).contains(current);
          if (needed) m->addToShotBreakdown(shot, current);
          else m->removeFromShotBreakdown(shot, current);
        }
        m->saveProjectDb();
        ZtoryDialogueHighlighter::rehighlightAll();
      });
    }
    // Toglie la parola dallo spoglio di TUTTO il progetto, non l'asset dal
    // Breakdown: quello si toglie dalla pagina Breakdown, dove si vede cosa
    // si toglie e da dove.
    QObject::connect(
        tagMenu->addAction(QObject::tr("Remove this tag (whole project)")),
        &QAction::triggered,
        [phrase]() { ztoryApplyWordTag(phrase, QString()); });
  }
}

// «Insert asset ▸ Prop ▸ SEDIA» (Franco, 2026-11-23: invece di scrivere la
// parola e poi segnarla). Scrive il nome dell'asset dove si e' cliccato e lo
// segna: e' lo stesso tag che si farebbe a mano, quindi vale per tutto il
// progetto e si toglie dallo stesso menu.
// pos: viewport coordinates, as customContextMenuRequested gives them.
inline void ztoryAddInsertAssetMenu(QMenu *menu, QTextEdit *field,
                                    const QPoint &pos) {
  if (field->isReadOnly()) return;
  ZtoryModel *m = ZtoryModel::instance();
  if (m->assets().empty()) return;
  QMenu *insMenu = menu->addMenu(QObject::tr("Insert asset"));
  const QString fieldShot = ztoryFieldShotUuid(field);
  if (!fieldShot.isEmpty()) {
    insMenu->addAction(QString::fromUtf8("\u2713 ") +
                       QObject::tr("= already in this shot's breakdown"))
        ->setEnabled(false);
    insMenu->addSeparator();
  }
  QStringList types;
  for (const Asset &a : m->assets())
    if (!types.contains(a.type, Qt::CaseInsensitive)) types << a.type;
  QPointer<QTextEdit> fld(field);
  for (const QString &type : types) {
    QMenu *sub = insMenu->addMenu(type.isEmpty() ? QObject::tr("Other") : type);
    for (const Asset &a : m->assets()) {
      if (a.type.compare(type, Qt::CaseInsensitive) != 0) continue;
      const QString name = a.name, uuid = a.uuid;
      QObject::connect(sub->addAction(ztoryAssetMenuText(a, fieldShot)),
                       &QAction::triggered,
                       [fld, pos, name, uuid]() {
        if (!fld) return;
        QTextCursor cur = fld->cursorForPosition(pos);
        // Spazi solo dove servono: il nome deve restare una parola intera,
        // o il riconoscimento non lo ritrova.
        const QString text = fld->toPlainText();
        const int at       = cur.position();
        auto wordChar = [](QChar ch) { return ch.isLetterOrNumber(); };
        QString ins = name;
        if (at > 0 && wordChar(text.at(at - 1))) ins.prepend(' ');
        if (at < text.size() && wordChar(text.at(at))) ins.append(' ');
        cur.insertText(ins);
        fld->setTextCursor(cur);
        ztoryApplyWordTag(name, uuid);
        // Lo shot di questo campo subito, anche se il modello non ha ancora
        // ricevuto il testo nuovo: all'uscita dal campo il resto torna.
        const QString shot = ztoryFieldShotUuid(fld);
        ZtoryModel *mm     = ZtoryModel::instance();
        if (!shot.isEmpty() && mm->addToShotBreakdown(shot, uuid))
          mm->saveProjectDb();
        fld->setFocus();
      });
    }
  }
}

// The Action and Notes menu: the standard one plus «Insert asset» and, on a
// selection, «Tag as».
inline void ztoryOfferWordTagMenu(QWidget *parent, QTextEdit *field,
                                  const QPoint &pos) {
  QMenu *menu = field->createStandardContextMenu();
  menu->addSeparator();
  ztoryAddInsertAssetMenu(menu, field, pos);
  const QString sel = field->textCursor().selectedText().trimmed();
  if (!sel.isEmpty()) ztoryAddWordTagMenu(menu, sel, field);
  menu->exec(field->viewport()->mapToGlobal(pos));
  delete menu;
}

