// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "backend/GraphSource.hpp"
#include "presentation/LayoutStore.hpp"
#include "presentation/StableListModel.hpp"

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <memory>

namespace wirerunner {

class GraphController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(StableListModel *cards READ cards CONSTANT)
  Q_PROPERTY(StableListModel *links READ links CONSTANT)
  Q_PROPERTY(QVariantList renderedLinks READ renderedLinks NOTIFY graphChanged)
  Q_PROPERTY(QVariantList anchors READ anchors NOTIFY graphChanged)
  Q_PROPERTY(QVariantList cardRects READ cardRects NOTIFY graphChanged)
  Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
  Q_PROPERTY(QString remoteSummary READ remoteSummary NOTIFY graphChanged)
  Q_PROPERTY(QString mediaFilter READ mediaFilter WRITE setMediaFilter NOTIFY mediaFilterChanged)
  Q_PROPERTY(QString selectedKey READ selectedKey NOTIFY selectionChanged)
  Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
  Q_PROPERTY(bool focusActive READ focusActive NOTIFY focusChanged)
  Q_PROPERTY(int cardCount READ cardCount NOTIFY graphChanged)
  Q_PROPERTY(int linkCount READ linkCount NOTIFY graphChanged)
  Q_PROPERTY(double canvasWidth READ canvasWidth NOTIFY graphChanged)
  Q_PROPERTY(double canvasHeight READ canvasHeight NOTIFY graphChanged)

public:
  explicit GraphController(std::unique_ptr<GraphSource> source, QString layoutPath = {}, QObject *parent = nullptr);
  ~GraphController() override;
  void start();

  StableListModel *cards() { return &cards_; }
  StableListModel *links() { return &links_; }
  QVariantList renderedLinks() const { return renderedLinks_; }
  QVariantList anchors() const { return anchors_; }
  QVariantList cardRects() const { return cardRects_; }
  QVariantMap selected() const { return selected_; }
  QString statusText() const { return statusText_; }
  QString remoteSummary() const { return remoteSummary_; }
  QString mediaFilter() const { return mediaFilter_; }
  QString selectedKey() const { return selectedKey_; }
  bool connected() const { return connected_; }
  bool focusActive() const { return !focusCards_.isEmpty(); }
  int cardCount() const { return cards_.count(); }
  int linkCount() const { return links_.count(); }
  double canvasWidth() const { return canvasWidth_; }
  double canvasHeight() const { return canvasHeight_; }

  void setMediaFilter(QString value);
  Q_INVOKABLE void moveCard(const QString &key, double x, double y);
  Q_INVOKABLE void toggleCard(const QString &key);
  Q_INVOKABLE void selectCard(const QString &key);
  Q_INVOKABLE void selectLink(const QString &key);
  Q_INVOKABLE void clearSelection();
  Q_INVOKABLE void focusSelected();
  Q_INVOKABLE void clearFocus();
  Q_INVOKABLE QVariantMap findCard(const QString &query);

signals:
  void graphChanged();
  void statusChanged();
  void selectionChanged();
  void focusChanged();
  void mediaFilterChanged();

private:
  struct CardState { QPointF position; bool expanded{}; bool persistent{}; };
  void applySnapshot(std::shared_ptr<const GraphSnapshot> snapshot);
  void applyStatus(SourceStatus status);
  void rebuildPresentation();
  void updateSelection();
  void saveCard(const QString &key);

  std::unique_ptr<GraphSource> source_;
  LayoutStore layoutStore_;
  StableListModel cards_;
  StableListModel links_;
  QVariantList anchors_;
  QVariantList cardRects_;
  QVariantList renderedLinks_;
  QVariantMap selected_;
  std::shared_ptr<const GraphSnapshot> snapshot_;
  ComposedGraph composed_;
  QHash<QString, CardState> cardStates_;
  QSet<QString> focusCards_;
  QString statusText_{"Starting…"};
  QString remoteSummary_;
  QString remoteName_;
  QString mediaFilter_{"all"};
  QString selectedKey_;
  QString selectedKind_;
  bool connected_{};
  double canvasWidth_{1000.0};
  double canvasHeight_{650.0};
};

} // namespace wirerunner
