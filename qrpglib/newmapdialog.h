#ifndef NEWMAPDIALOG_H
#define NEWMAPDIALOG_H 1

#include <QtGui>

class NewMapDialog : public QDialog {
  Q_OBJECT
public:
  NewMapDialog(QWidget * parent);
  QSpinBox * xspin;
  QSpinBox * yspin;
  QComboBox * tileset;
  QLineEdit * mapname;
public slots:
  int exec();
  void submit();
};

#endif