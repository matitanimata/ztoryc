#pragma once

//============================================================================
// ZtoryMouthApplyPopup — «da questo fotogramma a questo, usa questo set».
//
// La forma e' quella chiesta da Franco (2026-08-16): una tabella di TRATTI,
// non una scelta sola. Il motivo e' che durante la stessa battuta il
// personaggio puo' girarsi o cambiare espressione, e allora il set cambia a
// meta' frase — una scelta unica per battuta sarebbe sbagliata proprio nei
// casi che contano.
//
// All'apertura propone UNA riga sola, tutto l'intervallo col primo set: chi
// non deve cambiare niente fa un clic, chi deve spezza. Il costo lo paga solo
// chi ha il caso complicato.
//============================================================================

#include "ztorymouthapply.h"

#include "toonzqt/dvdialog.h"

#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

class ZtoryMouthApplyPopup final : public DVGui::Dialog {
  Q_OBJECT

public:
  ZtoryMouthApplyPopup();

protected:
  void showEvent(QShowEvent *) override;

private slots:
  void onTargetChanged(int);
  void onAddRange();
  void onRemoveRange();
  void onApply();

private:
  //! Riempie la tabella con una riga sola che copre tutta la colonna.
  void resetRanges();
  //! Sceglie il bersaglio che corrisponde alla colonna scelta, quando i
  //! nomi lo permettono. Se non corrisponde niente, non indovina.
  void pairColumnWithTarget();
  //! I tratti scritti nella tabella, scartando quelli senza senso.
  QVector<MouthApplyRange> readRanges(QString *why) const;
  //! Una cella con la tendina dei set del bersaglio corrente.
  QComboBox *makeSetCombo(const QString &current) const;

  QComboBox    *m_targetCombo = nullptr;
  QComboBox    *m_columnCombo = nullptr;
  QTableWidget *m_table       = nullptr;
  QPushButton  *m_addBt       = nullptr;
  QPushButton  *m_removeBt    = nullptr;
  QPushButton  *m_applyBt     = nullptr;
  QLabel       *m_note        = nullptr;

  QVector<MouthApplyTarget> m_targets;
  //! L'utente ha scelto il bersaglio a mano: da quel momento l'accoppiamento
  //! automatico non ci mette piu' becco.
  //!
  //! Serve davvero: se la scena del personaggio ha un nome che non corrisponde
  //! (un refuso, o una convenzione diversa) l'unica strada e' accoppiare a
  //! mano — e un automatismo che poi te la rovescia al primo cambio di colonna
  //! renderebbe la scelta manuale inutilizzabile (Franco, 2026-08-16).
  bool m_targetChosenByHand = false;
  //! Vero mentre e' l'accoppiamento a muovere la tendina, per non
  //! scambiare la propria scrittura per una scelta dell'utente.
  bool m_pairing = false;
  QVector<int>              m_phonemeCols;
};
