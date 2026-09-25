// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickPaintedItem>
#include <QVariantList>

namespace wirerunner {

class LinkLayer : public QQuickPaintedItem {
  Q_OBJECT
  Q_PROPERTY(QVariantList portAnchors READ portAnchors WRITE setPortAnchors NOTIFY portAnchorsChanged)
  Q_PROPERTY(QVariantList links READ links WRITE setLinks NOTIFY linksChanged)
  Q_PROPERTY(QVariantList blockers READ blockers WRITE setBlockers NOTIFY blockersChanged)
  Q_PROPERTY(double viewScale READ viewScale WRITE setViewScale NOTIFY viewTransformChanged)
  Q_PROPERTY(double contentX READ contentX WRITE setContentX NOTIFY viewTransformChanged)
  Q_PROPERTY(double contentY READ contentY WRITE setContentY NOTIFY viewTransformChanged)
  Q_PROPERTY(QString mediaFilter READ mediaFilter WRITE setMediaFilter NOTIFY mediaFilterChanged)
  Q_PROPERTY(QString selectedKey READ selectedKey WRITE setSelectedKey NOTIFY selectedKeyChanged)

public:
  explicit LinkLayer(QQuickItem *parent = nullptr);
  QVariantList portAnchors() const { return portAnchors_; }
  QVariantList links() const { return links_; }
  QVariantList blockers() const { return blockers_; }
  double viewScale() const { return viewScale_; }
  double contentX() const { return contentX_; }
  double contentY() const { return contentY_; }
  QString mediaFilter() const { return mediaFilter_; }
  QString selectedKey() const { return selectedKey_; }
  void setPortAnchors(QVariantList value);
  void setLinks(QVariantList value);
  void setBlockers(QVariantList value);
  void setViewScale(double value);
  void setContentX(double value);
  void setContentY(double value);
  void setMediaFilter(QString value);
  void setSelectedKey(QString value);

signals:
  void portAnchorsChanged();
  void linksChanged();
  void blockersChanged();
  void viewTransformChanged();
  void mediaFilterChanged();
  void selectedKeyChanged();
  void linkActivated(QString key);

protected:
  void paint(QPainter *painter) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  QVariantList portAnchors_;
  QVariantList links_;
  QVariantList blockers_;
  QString mediaFilter_{"all"};
  QString selectedKey_;
  double viewScale_{1.0};
  double contentX_{};
  double contentY_{};
};

} // namespace wirerunner
