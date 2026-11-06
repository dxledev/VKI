#pragma once
#include "core/theme.h"
#include <QPointer>
#include <QWidget>
class QBoxLayout;
class QLabel;
class QScrollArea;
class QPushButton;
namespace vki {
class ModalMotion;
class ModalBackdrop;
class Modal : public QWidget {
  Q_OBJECT
public:
  void relayout() { if (isOpen()) centerCard(); }
  Modal(QWidget *parent, QWidget *background);
  void open(const QString &title, const QString &body, const QStringList &actions);
  // Takes ownership of content and shows it inside the card.
  void openContent(const QString &title, QWidget *content, const QStringList &actions, bool wide = false,
                   bool inlineActions = false);
  void setTheme(const Theme &theme);
  void setActionEnabled(int index, bool enabled);
  void dismiss();
  void refreshSnapshot();
  bool isOpen() const { return isVisible() && !closing_; }
signals:
  void chosen(int index);
  void dismissed();

protected:
  bool event(QEvent *event) override;
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void hideEvent(QHideEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  bool eventFilter(QObject *object, QEvent *event) override;

private:
  QPointer<QWidget> background_, previousFocus_;
  QWidget *card_;
  QLabel *title_;
  ModalMotion *motion_;
  ModalBackdrop *backdrop_ = nullptr;
  bool closing_ = false, backgroundWasEnabled_ = true;
  int pendingChoice_ = -1;
  void beginDismiss(int choice);
  void finishDismiss();
  QScrollArea *bodyScroll_;
  bool wide_ = false;
  QList<QPushButton *> buttons_;
  QBoxLayout *actionBox_ = nullptr;
  bool inlineActions_ = false;
  QPixmap snapshot_, backgroundFrame_;
  bool backgroundFrozen_ = false, backgroundUpdatesWereEnabled_ = true;
  void freezeBackground();
  void restoreBackgroundUpdates();
  void composeSnapshot(const QPixmap &blurred);
  Theme theme_;
  void centerCard();
};
} // namespace vki
