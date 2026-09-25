// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QQuickItem>
#include <QVariantList>

namespace wirerunner {
class LinkLayer : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(QVariantList nodes READ nodes WRITE setNodes NOTIFY nodesChanged)
  Q_PROPERTY(QVariantList links READ links WRITE setLinks NOTIFY linksChanged)
  Q_PROPERTY(QString mediaFilter READ mediaFilter WRITE setMediaFilter NOTIFY mediaFilterChanged)
  Q_PROPERTY(quint32 selectedLinkId READ selectedLinkId WRITE setSelectedLinkId NOTIFY selectedLinkIdChanged)
public:
  explicit LinkLayer(QQuickItem *parent = nullptr);
  QVariantList nodes() const { return nodes_; }
  QVariantList links() const { return links_; }
  QString mediaFilter() const { return mediaFilter_; }
  quint32 selectedLinkId() const { return selectedLinkId_; }
  void setNodes(QVariantList value);
  void setLinks(QVariantList value);
  void setMediaFilter(QString value);
  void setSelectedLinkId(quint32 value);
signals:
  void nodesChanged();
  void linksChanged();
  void mediaFilterChanged();
  void selectedLinkIdChanged();
  void linkActivated(quint32 id, QString fromName, QString toName, QString media, QString state);
protected:
  QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
  void mousePressEvent(QMouseEvent *event) override;
private:
  QVariantList nodes_;
  QVariantList links_;
  QString mediaFilter_{"all"};
  quint32 selectedLinkId_{};
};
}
