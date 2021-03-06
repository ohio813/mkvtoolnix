#ifndef MTX_MKVTOOLNIX_GUI_MERGE_COMMAND_LINE_DIALOG_H
#define MTX_MKVTOOLNIX_GUI_MERGE_COMMAND_LINE_DIALOG_H

#include "common/common_pch.h"

#include <QAction>
#include <QDialog>

namespace mtx { namespace gui { namespace Merge {

namespace Ui {
class CommandLineDialog;
}

class CommandLineDialog : public QDialog {
  Q_OBJECT;

protected:
  // UI stuff:
  std::unique_ptr<Ui::CommandLineDialog> ui;
  QStringList const m_options;

public:
  explicit CommandLineDialog(QWidget *parent, QStringList const &options, QString const &title);
  ~CommandLineDialog();

public slots:
  void onEscapeModeChanged(int index);
  void copyToClipboard();
};

}}}

#endif // MTX_MKVTOOLNIX_GUI_MERGE_COMMAND_LINE_DIALOG_H
