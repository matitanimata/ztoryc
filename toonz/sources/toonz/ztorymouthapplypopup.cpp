#include "ztorymouthapplypopup.h"

#include "menubarcommandids.h"
#include "tapp.h"

#include "toonz/childstack.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcolumn.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobject.h"
#include "toonzqt/menubarcommand.h"

#include <QMainWindow>

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

//============================================================================

ZtoryMouthApplyPopup::ZtoryMouthApplyPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "ZtoryMouthApply") {
  setWindowTitle(tr("Assign Mouth Drawings"));
  setMinimumWidth(560);

  auto *top = new QWidget(this);
  auto *lay = new QVBoxLayout(top);
  lay->setContentsMargins(8, 8, 8, 8);
  lay->setSpacing(6);

  {
    auto *row = new QHBoxLayout();
    row->setSpacing(6);
    row->addWidget(new QLabel(tr("Phonemes:"), top));
    m_columnCombo = new QComboBox(top);
    m_columnCombo->setToolTip(
        tr("The column holding the phoneme names. One per character:\n"
           "pick the one belonging to the character you are animating."));
    row->addWidget(m_columnCombo, 1);
    row->addWidget(new QLabel(tr("Mouths:"), top));
    m_targetCombo = new QComboBox(top);
    m_targetCombo->setToolTip(
        tr("Where the mouths are. Only levels and sub-scenes that already\n"
           "have a mapping appear here — map them in ZtoRig ▸ Mouths first."));
    row->addWidget(m_targetCombo, 1);
    lay->addLayout(row);
  }

  m_table = new QTableWidget(0, 3, top);
  m_table->setHorizontalHeaderLabels(
      QStringList() << tr("From") << tr("To") << tr("Set"));
  m_table->verticalHeader()->setVisible(false);
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setMinimumHeight(140);
  lay->addWidget(m_table, 1);

  {
    auto *row = new QHBoxLayout();
    row->setSpacing(6);
    m_addBt    = new QPushButton(tr("+ Split"), top);
    m_removeBt = new QPushButton(tr("− Remove"), top);
    m_addBt->setToolTip(
        tr("Add a stretch. Use it where the character turns or changes mood\n"
           "halfway through a line and the mouths change with them."));
    row->addWidget(m_addBt);
    row->addWidget(m_removeBt);
    row->addStretch();
    lay->addLayout(row);
  }

  m_note = new QLabel(top);
  m_note->setWordWrap(true);
  lay->addWidget(m_note);

  m_applyBt = new QPushButton(tr("Apply"), this);
  addWidget(top);
  addButtonBarWidget(m_applyBt);

  connect(m_targetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ZtoryMouthApplyPopup::onTargetChanged);
  connect(m_columnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            pairColumnWithTarget();
            resetRanges();  // l'intervallo e' quello della colonna scelta
          });
  connect(m_addBt, &QPushButton::clicked, this,
          &ZtoryMouthApplyPopup::onAddRange);
  connect(m_removeBt, &QPushButton::clicked, this,
          &ZtoryMouthApplyPopup::onRemoveRange);
  connect(m_applyBt, &QPushButton::clicked, this,
          &ZtoryMouthApplyPopup::onApply);
}

//----------------------------------------------------------------------------

void ZtoryMouthApplyPopup::showEvent(QShowEvent *) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  // Lo stesso xsheet che usa il motore: quello CORRENTE. In uno storyboard le
  // colonne dei fonemi stanno dentro la sotto-scena dello shot.
  TXsheet *top      = ZtoryMouthApply::workingXsheet();

  // Riaprendo il popup si riparte pulito: la scelta manuale vale per la
  // sessione di lavoro, non per sempre.
  m_targetChosenByHand = false;
  m_targets     = ZtoryMouthApply::findTargets(scene);
  m_phonemeCols = ZtoryMouthApply::findPhonemeColumns(top);

  m_targetCombo->clear();
  for (const MouthApplyTarget &t : m_targets) m_targetCombo->addItem(t.label);

  m_columnCombo->clear();
  for (int col : m_phonemeCols) {
    // Il nome della colonna, se l'utente gliene ha dato uno: e' cosi' che
    // distingue quella di SOFIA da quella di MARIO.
    QString name = tr("Column %1").arg(col + 1);
    if (top)
      if (TStageObject *so = top->getStageObject(top->getColumnObjectId(col))) {
        const QString n = QString::fromStdString(so->getName());
        if (!n.isEmpty()) name = n;
      }
    m_columnCombo->addItem(name, col);
  }

  // Le due cose che mancano si dicono SEPARATE: si risolvono in posti diversi,
  // e un messaggio unico manderebbe l'utente a cercare nel posto sbagliato.
  QStringList missing;
  if (m_phonemeCols.isEmpty())
    // ⚠️ Il nome del comando sta SCRITTO qui dentro: rinominandolo (2026-08-17,
    // «Lip Sync from Storyboard Dialogue...» → «Lip Sync...») questo messaggio
    // ha continuato a mandare l'utente a cercare una voce di menu che non
    // esisteva piu'. Se il comando cambia nome di nuovo, cambia anche qui.
    missing << tr("no phoneme column here — generate them with Xsheet ▸ Lip "
                  "Sync…, which also says what else is missing.\n"
                  "In a storyboard the columns live INSIDE the shot: open the "
                  "shot's sub-scene first.");
  if (m_targets.isEmpty())
    missing << tr("no mapped mouths — map them in ZtoRig ▸ Mouths, on the "
                  "character's own drawings (once per character: the map "
                  "travels with them)");
  const bool ready = missing.isEmpty();
  m_applyBt->setEnabled(ready);
  m_table->setEnabled(ready);
  m_addBt->setEnabled(ready);
  m_removeBt->setEnabled(ready);
  m_note->setText(ready ? QString() : missing.join("\n"));

  if (ready) {
    pairColumnWithTarget();
    resetRanges();
  }
}

//----------------------------------------------------------------------------

void ZtoryMouthApplyPopup::pairColumnWithTarget() {
  // Scelto a mano: non si tocca. L'automatismo e' un suggerimento, non una
  // regola.
  if (m_targetChosenByHand) return;
  // ── ACCOPPIARE DA SOLI COLONNA E PERSONAGGIO ─────────────────────────────
  // Con due personaggi in scena ci sono due colonne di fonemi e due gruppi di
  // bocche, e sbagliare accoppiamento da' un lip sync perfetto sulla faccia
  // sbagliata. Le colonne generate portano il nome di chi parla, e le mappe
  // sanno di chi sono: quando i due nomi si toccano, si sceglie da soli.
  //
  // Se non si toccano NON si indovina: restano le due tendine come le ha
  // lasciate l'utente.
  const QString colName = m_columnCombo->currentText();
  if (colName.isEmpty()) return;
  for (int i = 0; i < m_targets.size(); i++) {
    const QString who = m_targets[i].map.characterName;
    // Il nome del personaggio compare nel nome della colonna («SOFIA mouths»)
    // e, per i set recuperati da libreria, dentro l'etichetta del bersaglio.
    const bool hit =
        (!who.isEmpty() && colName.contains(who, Qt::CaseInsensitive)) ||
        m_targets[i].label.contains(colName.section(' ', 0, 0),
                                    Qt::CaseInsensitive);
    if (!hit) continue;
    m_pairing = true;
    m_targetCombo->setCurrentIndex(i);
    m_pairing = false;
    return;
  }
}

//----------------------------------------------------------------------------

QComboBox *ZtoryMouthApplyPopup::makeSetCombo(const QString &current) const {
  auto *cb = new QComboBox();
  const int ti = m_targetCombo->currentIndex();
  if (ti >= 0 && ti < m_targets.size())
    for (const MouthSet &ms : m_targets[ti].map.sets) cb->addItem(ms.name);
  const int i = cb->findText(current);
  if (i >= 0) cb->setCurrentIndex(i);
  return cb;
}

void ZtoryMouthApplyPopup::resetRanges() {
  m_table->setRowCount(0);
  if (m_phonemeCols.isEmpty() || m_targets.isEmpty()) return;

  TXsheet *top  = ZtoryMouthApply::workingXsheet();
  const int col = m_columnCombo->currentData().toInt();
  int r0 = 0, r1 = 0;
  if (top && top->getColumn(col)) top->getColumn(col)->getRange(r0, r1);

  // Una riga sola che copre tutta la colonna: chi non deve cambiare set fa un
  // clic e basta.
  m_table->insertRow(0);
  auto *from = new QSpinBox();
  from->setRange(1, 100000);
  from->setValue(r0 + 1);
  auto *to = new QSpinBox();
  to->setRange(1, 100000);
  to->setValue(r1 + 1);
  m_table->setCellWidget(0, 0, from);
  m_table->setCellWidget(0, 1, to);
  m_table->setCellWidget(0, 2, makeSetCombo(QString()));
}

void ZtoryMouthApplyPopup::onTargetChanged(int) {
  // Arrivato dall'utente e non da pairColumnWithTarget (che scrive con la
  // tendina bloccata): da qui in poi comanda lui.
  if (!m_pairing) m_targetChosenByHand = true;
  // Cambiando bersaglio cambiano i set disponibili: le tendine vecchie
  // punterebbero a nomi che qui non esistono.
  for (int r = 0; r < m_table->rowCount(); r++) {
    auto *old = qobject_cast<QComboBox *>(m_table->cellWidget(r, 2));
    m_table->setCellWidget(r, 2, makeSetCombo(old ? old->currentText()
                                                  : QString()));
  }
}

void ZtoryMouthApplyPopup::onAddRange() {
  const int r = m_table->rowCount();
  m_table->insertRow(r);
  auto *from = new QSpinBox();
  from->setRange(1, 100000);
  auto *to = new QSpinBox();
  to->setRange(1, 100000);
  // Il tratto nuovo parte dove finisce il precedente: spezzare una battuta e'
  // il caso normale, e ricominciare da 1 costringerebbe a ridigitare sempre.
  if (r > 0) {
    if (auto *prevTo = qobject_cast<QSpinBox *>(m_table->cellWidget(r - 1, 1))) {
      from->setValue(prevTo->value() + 1);
      to->setValue(prevTo->value() + 1);
    }
  }
  m_table->setCellWidget(r, 0, from);
  m_table->setCellWidget(r, 1, to);
  m_table->setCellWidget(r, 2, makeSetCombo(QString()));
}

void ZtoryMouthApplyPopup::onRemoveRange() {
  const int r = m_table->currentRow();
  if (r >= 0) m_table->removeRow(r);
}

//----------------------------------------------------------------------------

QVector<MouthApplyRange> ZtoryMouthApplyPopup::readRanges(QString *why) const {
  QVector<MouthApplyRange> out;
  for (int r = 0; r < m_table->rowCount(); r++) {
    auto *from = qobject_cast<QSpinBox *>(m_table->cellWidget(r, 0));
    auto *to   = qobject_cast<QSpinBox *>(m_table->cellWidget(r, 1));
    auto *set  = qobject_cast<QComboBox *>(m_table->cellWidget(r, 2));
    if (!from || !to || !set) continue;
    if (set->currentText().isEmpty()) continue;
    // Un tratto rovesciato e' quasi sempre un refuso: si dice invece di
    // ignorarlo in silenzio, o l'utente crederebbe applicato un pezzo che non
    // lo e'.
    if (to->value() < from->value()) {
      if (why)
        *why = tr("Row %1 ends before it starts (%2 → %3).")
                   .arg(r + 1).arg(from->value()).arg(to->value());
      return QVector<MouthApplyRange>();
    }
    MouthApplyRange rg;
    rg.from    = from->value();
    rg.to      = to->value();
    rg.setName = set->currentText();
    out.push_back(rg);
  }
  return out;
}

void ZtoryMouthApplyPopup::onApply() {
  const int ti = m_targetCombo->currentIndex();
  if (ti < 0 || ti >= m_targets.size()) return;

  QString why;
  const QVector<MouthApplyRange> ranges = readRanges(&why);
  if (!why.isEmpty()) {
    m_note->setText(why);
    return;
  }
  if (ranges.isEmpty()) {
    m_note->setText(tr("Nothing to apply: add a stretch first."));
    return;
  }

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  const int col     = m_columnCombo->currentData().toInt();
  const MouthApplyReport rep =
      ZtoryMouthApply::apply(scene, col, m_targets[ti], ranges);

  // Il resoconto resta nel popup invece di sparire in una finestra da chiudere:
  // se ci sono scontri l'utente deve poterli leggere mentre guarda l'xsheet.
  m_note->setText(rep.summary());
}

//============================================================================

OpenPopupCommandHandler<ZtoryMouthApplyPopup> openZtoryMouthApply(
    MI_ZtoryApplyMouths);
