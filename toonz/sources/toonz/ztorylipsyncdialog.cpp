#include "ztorylipsyncdialog.h"

#include "ztorylipsync.h"
#include "ztorymodel.h"
#include "ztorymouthapply.h"
#include "ztorycharacter.h"
#include "menubarcommandids.h"
#include "tapp.h"

#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "tundo.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// Verde e ambra come nel pannello Breakdown: la stessa coppia di colori per la
// stessa domanda («questo si risolve?») invece di due convenzioni.
const char *kOk   = "color:#22D160;";
const char *kMiss = "color:#F5A623;";

//----------------------------------------------------------------------------
// Il copione che manca: si scrive QUI.
//
// Rifiutarsi e mandare l'utente nel Board a riempire i pannelli era un
// controsenso: siamo dentro lo shot, sappiamo che il testo manca, e il testo e'
// la cosa che rende il lip sync preciso. Anche Rhubarb ha il suo campo per il
// copione, e con il testo lavora meglio.
//
// Ritorna: true = procedere. `text` vuoto = l'utente ha scelto il suono.
//----------------------------------------------------------------------------
bool askDialogue(QWidget *parent, bool ownScene, QString *text) {
  QDialog dlg(parent);
  dlg.setWindowTitle(QObject::tr("No dialogue in this shot"));
  dlg.setMinimumWidth(520);
  auto *lay = new QVBoxLayout(&dlg);
  auto *txt = new QLabel(
      QObject::tr(
          "This shot's panels have no dialogue. Write it here and the words "
          "become the ones in the columns, timed on the audio — which is the "
          "accurate way. Leave it empty to read the mouth shapes from the "
          "sound alone."),
      &dlg);
  txt->setWordWrap(true);
  lay->addWidget(txt);

  auto *edit = new QPlainTextEdit(&dlg);
  edit->setPlaceholderText(
      QObject::tr("SOFIA: and where do you think you are going?"));
  edit->setMinimumHeight(110);
  lay->addWidget(edit);

  auto *hint = new QLabel(
      ownScene
          ? QObject::tr(
                "A name in capitals followed by a colon says who speaks — that "
                "is how each character gets their own columns and their own "
                "mouths. In an exported shot there are no storyboard panels to "
                "keep it in, so this text is used for this pass only.")
          : QObject::tr(
                "A name in capitals followed by a colon says who speaks — that "
                "is how each character gets their own columns and their own "
                "mouths. It is saved in the shot's first panel, so the Board "
                "and the PDF have it too."),
      &dlg);
  hint->setWordWrap(true);
  hint->setStyleSheet("color:#999; font-size:11px;");
  lay->addWidget(hint);

  auto *bbox = new QDialogButtonBox(&dlg);
  QPushButton *useText =
      bbox->addButton(QObject::tr("Use this text"), QDialogButtonBox::AcceptRole);
  QPushButton *noText = bbox->addButton(QObject::tr("Sound only (Rhubarb)"),
                                        QDialogButtonBox::ActionRole);
  bbox->addButton(QDialogButtonBox::Cancel);
  lay->addWidget(bbox);

  bool soundOnly = false;
  QObject::connect(useText, &QPushButton::clicked, &dlg, [&] {
    if (edit->toPlainText().trimmed().isEmpty()) {
      DVGui::warning(QObject::tr(
          "Nothing written yet. Type the dialogue, or choose «Sound only»."));
      return;
    }
    dlg.accept();
  });
  QObject::connect(noText, &QPushButton::clicked, &dlg, [&] {
    soundOnly = true;
    dlg.accept();
  });
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return false;

  *text = soundOnly ? QString() : edit->toPlainText().trimmed();
  return true;
}

//----------------------------------------------------------------------------
// Chi parla, quando si e' scelto il suono.
//
// Stesso modo con cui si scelgono i personaggi altrove (la tendina di New Scene
// in modalita' Character): gli asset esistenti, piu' la possibilita' di
// aggiungerne uno. Senza copione nessuno puo' dire chi parla, e la catena verso
// i set di bocche parte da li': chiederlo e' l'unica alternativa a una colonna
// anonima.
//----------------------------------------------------------------------------
bool askCharacter(QWidget *parent, QString *uuid, QString *name) {
  ZtoryModel *m = ZtoryModel::instance();

  QDialog dlg(parent);
  dlg.setWindowTitle(QObject::tr("Who is speaking?"));
  dlg.setMinimumWidth(420);
  auto *lay = new QVBoxLayout(&dlg);
  auto *txt = new QLabel(
      QObject::tr("This shot has no dialogue written in its panels, so the "
                  "mouth shapes come from the sound alone and nothing says who "
                  "is speaking. Pick the character the columns belong to."),
      &dlg);
  txt->setWordWrap(true);
  lay->addWidget(txt);

  auto *combo = new QComboBox(&dlg);
  for (int i = 0; i < m->assetCount(); i++) {
    const Asset &a = m->assets()[i];
    if (a.type.compare("Character", Qt::CaseInsensitive) != 0) continue;
    combo->addItem(a.name, a.uuid);
  }
  // L'aggiunta a mano non e' un ripiego: il personaggio disegnato adesso, che
  // nel tracker non c'e' ancora, e' un caso normale in animazione.
  combo->addItem(QObject::tr("＋ Add character…"), QString("+"));
  lay->addWidget(combo);

  auto *box =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(box);
  QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return false;

  if (combo->currentData().toString() == "+") {
    bool ok = false;
    const QString nm = QInputDialog::getText(
        parent, QObject::tr("Add character"), QObject::tr("Character name:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || nm.trimmed().isEmpty()) return false;
    // Registrato fra gli asset: cosi' la volta dopo e' in elenco, e il suo set
    // di bocche puo' agganciarsi a lui invece di restare orfano.
    m->addAsset("Character", nm.trimmed());
    *name = nm.trimmed();
    *uuid =
        m->assetCount() > 0 ? m->assets()[m->assetCount() - 1].uuid : QString();
    return true;
  }
  *uuid = combo->currentData().toString();
  *name = combo->currentText();
  return true;
}

}  // namespace

//----------------------------------------------------------------------------

void ztoryShowLipSyncDialog(QWidget *parent) {
  const QString why = ZtoryLipSync::unavailableReason();
  if (!why.isEmpty()) { DVGui::warning(why); return; }

  if (!ztoryCurrentShotContext().isValid()) {
    // I DUE posti dove si puo' fare, detti entrambi: prima il messaggio parlava
    // solo di sotto-scene, e nella scena di uno shot esportato — che e' dove si
    // animano le bocche — sembrava che il comando non funzionasse.
    DVGui::warning(QObject::tr(
        "Nothing to work on here. Lip sync works in two places:\n\n"
        "\xE2\x80\xA2 inside a shot's sub-scene in the storyboard (the words "
        "come from its panels)\n"
        "\xE2\x80\xA2 in an exported shot scene, where the shot is the scene "
        "itself\n\n"
        "In the storyboard's main xsheet there is no single shot to write the "
        "columns into."));
    return;
  }

  // ⚠️ NON MODALE, e con nessun puntatore conservato.
  //
  // Franco, 2026-08-17: «il pop-up dovrebbe restare aperto finche' i 3 passaggi
  // non diventano tutti verdi». Ha ragione: e' una lista di cose da fare, e una
  // lista che si chiude a ogni spunta obbliga a riaprirla tre volte. Ma per
  // restare aperta mentre si mappa in ZtoRig deve lasciar lavorare — modale,
  // bloccherebbe proprio il pannello a cui manda.
  //
  // Il prezzo del non modale e' che la scena puo' cambiare sotto: un TXsheet*
  // tenuto da parte diventerebbe un puntatore morto, cioe' un crash. Quindi il
  // contesto NON si conserva — si ricalcola a ogni aggiornamento e a ogni clic,
  // e se non e' piu' valido la finestra si chiude da sola.
  auto *dlg = new QDialog(parent);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(QObject::tr("Lip Sync"));
  dlg->setMinimumWidth(620);

  auto *lay  = new QVBoxLayout(dlg);
  auto *head = new QLabel(dlg);
  head->setStyleSheet("font-weight:bold;");
  lay->addWidget(head);
  auto *grid = new QGridLayout();
  lay->addLayout(grid);

  struct Row {
    QLabel *what      = nullptr;
    QLabel *state     = nullptr;
    QPushButton *btn  = nullptr;
  };
  auto makeRow = [&](int r, const QString &what) -> Row {
    Row row;
    row.what = new QLabel(what, dlg);
    row.what->setStyleSheet("font-weight:bold;");
    row.state = new QLabel(dlg);
    row.state->setWordWrap(true);
    row.btn = new QPushButton(dlg);
    grid->addWidget(row.what, r, 0, Qt::AlignTop);
    grid->addWidget(row.state, r, 1);
    grid->addWidget(row.btn, r, 2, Qt::AlignTop);
    grid->setColumnStretch(1, 1);
    return row;
  };
  Row rGen = makeRow(0, QObject::tr("Dialogue columns"));
  Row rMap = makeRow(1, QObject::tr("Mouth sets"));
  Row rSet = makeRow(2, QObject::tr("Mouth set on frames"));

  auto *hint = new QLabel(
      QObject::tr("The three steps are separate on purpose: the timing comes "
                  "from the sound, the mapping belongs to the drawings (once "
                  "per character), and which set to use changes when the "
                  "character turns.\n"
                  "This window stays open and follows what you do: map in "
                  "ZtoRig and come back, the rows update by themselves."),
      dlg);
  hint->setWordWrap(true);
  hint->setStyleSheet("color:#999; font-size:11px;");
  lay->addWidget(hint);

  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
  QPushButton *closeBt = bbox->button(QDialogButtonBox::Close);
  lay->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::close);

  // Se i set sono stati scelti almeno una volta, il lavoro e' fatto e il posto
  // d'onore passa a Close (Franco, 2026-08-17). Non si SPEGNE il bottone,
  // pero': applicare un secondo set su un altro tratto — frontale nella prima
  // meta', tre quarti quando si gira — e' esattamente come si lavora. Spento
  // impedirebbe il caso normale per segnalare la fine di quello semplice.
  auto *assigned = new bool(false);
  QObject::connect(dlg, &QObject::destroyed, [assigned] { delete assigned; });

  // ── Lo stato, riletto da zero ogni volta ────────────────────────────────
  auto refresh = [=]() {
    const ZtoryShotContext ctx = ztoryCurrentShotContext();
    if (!ctx.isValid()) {
      // Cambiata scena, o usciti dalla sotto-scena: la finestra non ha piu' un
      // soggetto. Chiuderla e' l'unica risposta onesta — mostrare lo stato di
      // uno shot che non e' piu' quello aperto sarebbe peggio del silenzio.
      dlg->close();
      return;
    }
    ZtoryModel *m     = ZtoryModel::instance();
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) { dlg->close(); return; }

    // ⚠️ Nella scena dello shot esportato i PANNELLI NON CI SONO: il modello
    // non tiene gli shot dello storyboard, e leggere m->shot(0) sarebbe un
    // accesso fuori dai limiti.
    QString shotName, dialogue;
    if (ctx.ownScene) {
      shotName = QString::fromStdWString(scene->getScenePath().getWideName());
    } else if (ctx.shotIndex >= 0 && ctx.shotIndex < m->shotCount()) {
      const ShotData &sd = m->shot(ctx.shotIndex);
      shotName           = sd.label();
      dialogue           = ztoryShotDialogue(sd.panels);
    }
    head->setText(shotName);

    // Le colonne si contano dal CONTENUTO e non dal nome: una colonna
    // rinominata a mano resta una colonna di viseme, e dire «non ci sono»
    // mentre ci sono e' il peggior errore in una finestra di stato.
    const QVector<int> phon = ZtoryMouthApply::findPhonemeColumns(ctx.subXsheet);
    const bool hasCols      = !phon.isEmpty();
    const QVector<MouthApplyTarget> targets = ZtoryMouthApply::findTargets(scene);
    const bool hasSets                      = !targets.isEmpty();

    rGen.state->setText(
        hasCols ? QObject::tr("%1 column(s) in this shot").arg(phon.size())
                : dialogue.trimmed().isEmpty()
                      ? QObject::tr("none — no written dialogue either, so the "
                                    "timing would come from the sound")
                      : QObject::tr("none — the words are in the panels, ready "
                                    "to be timed"));
    rGen.state->setStyleSheet(hasCols ? kOk : kMiss);
    rGen.btn->setText(hasCols ? QObject::tr("Regenerate…")
                              : QObject::tr("Generate…"));

    QStringList who;
    for (const MouthApplyTarget &t : targets)
      if (!who.contains(t.label)) who << t.label;
    rMap.state->setText(hasSets
                            ? who.join(", ")
                            : QObject::tr("none reachable from this shot — map "
                                          "the mouth drawings once, and they "
                                          "travel with the character"));
    rMap.state->setStyleSheet(hasSets ? kOk : kMiss);
    rMap.btn->setText(hasSets ? QObject::tr("Map more…")
                              : QObject::tr("Map mouths…"));

    const bool ready = hasCols && hasSets;
    rSet.state->setText(ready
                            ? QObject::tr("ready: pick the set and the frames")
                            : QObject::tr("needs the columns and a mapped set"));
    rSet.state->setStyleSheet(ready ? kOk : kMiss);
    // Franco, 2026-08-17: «forse sarebbe piu' giusto indicare come "choose
    // mouth set"». Dal punto di vista di chi clicca, qui si scegle QUALE set su
    // QUALI fotogrammi: «Assign drawings» diceva il risultato, non la scelta.
    if (*assigned) {
      rSet.state->setText(QObject::tr(
          "done — choose another set if the character turns and needs a "
          "different one"));
      rSet.state->setStyleSheet(kOk);
      rSet.btn->setText(QObject::tr("Choose another set…"));
      // Close diventa il bottone d'onore: premendo Invio si chiude, che e' cio'
      // che si vuole fare quando le tre righe sono verdi.
      if (closeBt) {
        closeBt->setDefault(true);
        closeBt->setFocus();
      }
    } else {
      rSet.btn->setText(QObject::tr("Choose mouth set…"));
    }
    rSet.btn->setEnabled(ready);
  };

  // ── Generate ────────────────────────────────────────────────────────────
  QObject::connect(rGen.btn, &QPushButton::clicked, dlg, [=]() {
    const ZtoryShotContext ctx = ztoryCurrentShotContext();
    if (!ctx.isValid()) { dlg->close(); return; }
    ZtoryModel *m = ZtoryModel::instance();

    QString script;
    if (!ctx.ownScene && ctx.shotIndex >= 0 && ctx.shotIndex < m->shotCount())
      script = ztoryShotDialogue(m->shot(ctx.shotIndex).panels);

    ZtoryLipSync::Request req;
    if (script.trimmed().isEmpty()) {
      // Prima si offre di SCRIVERLO: il testo e' cio' che rende i tempi
      // precisi, e chiedere il personaggio senza aver prima chiesto il testo
      // sarebbe partire dalla soluzione di ripiego.
      QString typed;
      if (!askDialogue(dlg, ctx.ownScene, &typed)) return;
      if (!typed.isEmpty()) {
        script = typed;
        // Salvato nel primo pannello SOLO se i pannelli esistono, cioe' nello
        // storyboard. Nello shot esportato il dialogo non ha un posto dove
        // stare, e il testo serve questa passata e basta.
        if (!ctx.ownScene && ctx.shotIndex >= 0 &&
            ctx.shotIndex < m->shotCount()) {
          ShotData &live = m->shot(ctx.shotIndex);
          if (live.panels.empty()) live.panels.push_back(PanelData());
          live.panels[0].dialog = typed;
          TApp::instance()->getCurrentScene()->setDirtyFlag(true);
        }
      } else {
        QString uuid, name;
        if (!askCharacter(dlg, &uuid, &name)) return;
        req.characterUuid = uuid;
        req.characterName = name;
      }
    }

    const QString err =
        ztoryPrepareLipSync(ctx, script, req, /*allowWithoutScript=*/true);
    if (!err.isEmpty()) { DVGui::warning(err); return; }

    QVector<ZtoryCharacterTrack> tracks;
    QString msg;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = ztoryRunLipSyncBlocking(req, tracks, msg);
    QApplication::restoreOverrideCursor();
    if (!ok) { DVGui::warning(msg); return; }

    TUndoManager::manager()->beginBlock();
    for (int c : ztoryFindLipSyncColumns(ctx.subXsheet))
      ctx.subXsheet->removeColumn(c);
    int orphans = 0;
    const int n =
        ztoryWriteLipSyncColumns(ctx.subXsheet, tracks,
                                 ctx.lastRow - ctx.firstRow + 1, &orphans)
            .size();
    TUndoManager::manager()->endBlock();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    QString extra;
    if (orphans > 0)
      extra = QObject::tr(
                  "  —  %1 words have no character: check the names in the "
                  "dialogue (they turn green when recognised).")
                  .arg(orphans);
    DVGui::info(QObject::tr("%1  —  %2 columns written.").arg(msg).arg(n) +
                extra);
    // La finestra RESTA aperta: la riga diventa verde e si passa alla
    // successiva.
    refresh();
  });

  // ── Map: apre ZtoRig e NON si chiude ────────────────────────────────────
  QObject::connect(rMap.btn, &QPushButton::clicked, dlg, [=]() {
    CommandManager::instance()->execute(MI_OpenZtoRig);
    // Si mappa la' e si torna qui: la riga si aggiorna da sola, perche' salvare
    // un set emette le notifiche a cui questa finestra e' agganciata.
  });

  // ── Choose mouth set: il popup e' modale, quindi al ritorno si rilegge ──
  QObject::connect(rSet.btn, &QPushButton::clicked, dlg, [=]() {
    CommandManager::instance()->execute(MI_ZtoryApplyMouths);
    // ⚠️ NON si chiude da sola, e non e' pigrizia: la finestra esiste per
    // mostrare che le tre righe sono verdi, e chiudersi proprio nel momento in
    // cui lo diventano nasconderebbe l'unica cosa che aveva da dire. Chiude
    // l'utente, quando ha visto.
    *assigned = true;
    refresh();
  });

  // Le righe seguono la scena: mappando in ZtoRig arrivano queste notifiche, ed
  // e' cio' che fa diventare verde la riga senza riaprire niente.
  QObject::connect(TApp::instance()->getCurrentXsheet(),
                   &TXsheetHandle::xsheetChanged, dlg, refresh);
  QObject::connect(TApp::instance()->getCurrentScene(),
                   &TSceneHandle::castChanged, dlg, refresh);
  QObject::connect(TApp::instance()->getCurrentScene(),
                   &TSceneHandle::sceneSwitched, dlg, refresh);

  refresh();
  dlg->show();
  dlg->raise();
}
