#pragma once

//============================================================================
// ZtoryDialogueCompleter — completes a character's name while it is typed at
// the start of a line of the dialogue field (Franco, 2026-11-22: «appena
// comincio a scrivere il nome, riconosca il personaggio e proponga di
// completarlo»).
//
// The popup lists the project's characters whose name CONTAINS what was typed
// (case and accents ignored): «scien» finds GUIDA-SCIENZIATO too. Return or
// Tab writes «NAME: » — the colon form, the one recognised on a single line
// whatever the case — and the highlighter turns it green.
//
// It opens only while typing one character at a time at the end of a line
// that has no colon yet: a pasted script must not pop a list up.
//
// No Q_OBJECT on purpose: lambdas and an eventFilter override need no moc, so
// this header stands alone without touching CMakeLists.
//============================================================================

#include "ztorymodel.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

class ZtoryDialogueCompleter final : public QObject {
public:
  explicit ZtoryDialogueCompleter(QTextEdit *field)
      : QObject(field), m_field(field) {
    m_model     = new QStringListModel(this);
    m_completer = new QCompleter(m_model, this);
    m_completer->setWidget(field);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    // Ours runs BEFORE the completer's own filter (installed later = called
    // first): the completer would also hand Return to the field, which would
    // write a new line after the name.
    m_completer->popup()->installEventFilter(this);
    connect(m_completer,
            QOverload<const QString &>::of(&QCompleter::activated), this,
            [this](const QString &name) { insertName(name); });
    connect(field->document(), &QTextDocument::contentsChange, this,
            [this](int, int removed, int added) {
              // removed == added: the highlighter recolouring, not an edit.
              if (m_inserting || removed == added) return;
              // One character typed or deleted: the user is writing.
              if (added == 1 || (added == 0 && removed == 1))
                update();
              else
                m_completer->popup()->hide();
            });
  }

protected:
  bool eventFilter(QObject *obj, QEvent *e) override {
    if (obj != m_completer->popup() || e->type() != QEvent::KeyPress)
      return QObject::eventFilter(obj, e);
    QAbstractItemView *pop = m_completer->popup();
    switch (static_cast<QKeyEvent *>(e)->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab: {
      QModelIndex i = pop->currentIndex();
      if (!i.isValid()) i = m_completer->completionModel()->index(0, 0);
      pop->hide();
      if (i.isValid()) insertName(i.data().toString());
      return true;
    }
    case Qt::Key_Escape:
      pop->hide();
      return true;
    default:
      return false;  // letters go on to the field through the completer
    }
  }

private:
  // What has been typed on this line, if it can still be a name.
  QString typedPrefix(QTextCursor *lineStart) const {
    QTextCursor c = m_field->textCursor();
    if (c.hasSelection()) return QString();
    const QTextBlock b = c.block();
    const int col      = c.positionInBlock();
    if (col != b.text().length()) return QString();  // not at the line's end
    const QString typed = b.text().left(col);
    if (typed.contains(':') || typed.length() > 40) return QString();
    if (lineStart) {
      *lineStart = c;
      lineStart->setPosition(b.position());
    }
    return typed.trimmed();
  }

  void update() {
    QAbstractItemView *pop = m_completer->popup();
    const QString typed    = typedPrefix(nullptr);
    if (typed.length() < 2) {
      pop->hide();
      return;
    }
    QStringList names;
    for (const Asset &a : ZtoryModel::instance()->assets())
      if (ZtoryModel::isCharacterType(a.type) && !names.contains(a.name))
        names << a.name;
    names.sort(Qt::CaseInsensitive);
    m_model->setStringList(names);
    m_completer->setCompletionPrefix(typed);
    const int n = m_completer->completionCount();
    // Already written in full: nothing to propose.
    if (n == 0 ||
        (n == 1 && m_completer->currentCompletion().compare(
                       typed, Qt::CaseInsensitive) == 0)) {
      pop->hide();
      return;
    }
    pop->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    QRect r = m_field->cursorRect();
    r.setWidth(pop->sizeHintForColumn(0) +
               pop->verticalScrollBar()->sizeHint().width() + 8);
    m_completer->complete(r);
  }

  void insertName(const QString &name) {
    QTextCursor start;
    if (typedPrefix(&start).isEmpty()) return;
    QTextCursor c = m_field->textCursor();
    c.setPosition(start.position(), QTextCursor::KeepAnchor);
    m_inserting = true;
    c.insertText(name + ": ");
    m_inserting = false;
    m_field->setTextCursor(c);
  }

  QTextEdit *m_field;
  QStringListModel *m_model;
  QCompleter *m_completer;
  bool m_inserting = false;
};
