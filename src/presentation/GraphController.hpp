// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "backend/GraphSource.hpp"

#include <QObject>
#include <QVariantList>
#include <memory>

namespace wirerunner {

class GraphController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList nodes READ nodes NOTIFY graphChanged)
  Q_PROPERTY(QVariantList links READ links NOTIFY graphChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
  Q_PROPERTY(QString remoteSummary READ remoteSummary NOTIFY graphChanged)
  Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
  Q_PROPERTY(int nodeCount READ nodeCount NOTIFY graphChanged)
  Q_PROPERTY(int linkCount READ linkCount NOTIFY graphChanged)

public:
  explicit GraphController(std::unique_ptr<GraphSource> source, QObject *parent = nullptr);
  ~GraphController() override;
  void start();
  QVariantList nodes() const { return nodes_; }
  QVariantList links() const { return links_; }
  QString statusText() const { return statusText_; }
  QString remoteSummary() const { return remoteSummary_; }
  bool connected() const { return connected_; }
  int nodeCount() const { return static_cast<int>(nodes_.size()); }
  int linkCount() const { return static_cast<int>(links_.size()); }

signals:
  void graphChanged();
  void statusChanged();

private:
  void applySnapshot(std::shared_ptr<const GraphSnapshot> snapshot);
  void applyStatus(SourceStatus status);
  std::unique_ptr<GraphSource> source_;
  QVariantList nodes_;
  QVariantList links_;
  QString statusText_{"Starting…"};
  QString remoteSummary_;
  bool connected_{};
};
}
