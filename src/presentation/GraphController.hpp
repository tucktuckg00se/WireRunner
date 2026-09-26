// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "backend/GraphSource.hpp"
#include "presentation/LayoutStore.hpp"
#include "presentation/StableListModel.hpp"

#include <QObject>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <memory>
#include <optional>

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
  Q_PROPERTY(QVariantMap routing READ routing NOTIFY routingChanged)
  Q_PROPERTY(bool feedbackConfirmation READ feedbackConfirmation NOTIFY feedbackConfirmationChanged)
  Q_PROPERTY(QString noticeText READ noticeText NOTIFY noticeChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
  Q_PROPERTY(bool commandPending READ commandPending NOTIFY commandPendingChanged)

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
  QVariantMap routing() const { return routing_; }
  bool feedbackConfirmation() const { return feedbackConfirmation_; }
  QString noticeText() const { return noticeText_; }
  bool canUndo() const;
  bool canRedo() const;
  bool commandPending() const { return pending_.has_value(); }

  void setMediaFilter(QString value);
  Q_INVOKABLE void moveCard(const QString &key, double x, double y);
  Q_INVOKABLE void toggleCard(const QString &key);
  Q_INVOKABLE void selectCard(const QString &key);
  Q_INVOKABLE void selectLink(const QString &key);
  Q_INVOKABLE void clearSelection();
  Q_INVOKABLE void focusSelected();
  Q_INVOKABLE void clearFocus();
  Q_INVOKABLE QVariantMap findCard(const QString &query);
  Q_INVOKABLE void expandForRouting(const QString &cardKey);
  Q_INVOKABLE void beginRoute(quint32 outputPortId, double x, double y);
  Q_INVOKABLE void updateRoute(double x, double y);
  Q_INVOKABLE void finishRoute(double x, double y);
  Q_INVOKABLE void finishRouteToPort(quint32 inputPortId);
  Q_INVOKABLE void cancelRoute();
  Q_INVOKABLE void confirmFeedback();
  Q_INVOKABLE void cancelFeedback();
  Q_INVOKABLE void disconnectSelected();
  Q_INVOKABLE void setNodeVolume(quint32 nodeId, double percent);
  Q_INVOKABLE void setNodeMuted(quint32 nodeId, bool muted);
  Q_INVOKABLE void undo();
  Q_INVOKABLE void redo();

signals:
  void graphChanged();
  void statusChanged();
  void selectionChanged();
  void focusChanged();
  void mediaFilterChanged();
  void routingChanged();
  void feedbackConfirmationChanged();
  void noticeChanged();
  void historyChanged();
  void commandPendingChanged();

private:
  struct CardState { QPointF position; bool expanded{}; bool persistent{}; };
  enum class HistoryKind { Created, Destroyed };
  enum class OperationIntent { Normal, Undo, Redo };
  struct HistoryAction { HistoryKind kind; GraphLink link; qint64 recordedAt{}; };
  struct PendingOperation {
    CommandId commandId{};
    bool creating{};
    GraphLink link;
    OperationIntent intent{OperationIntent::Normal};
  };
  struct PendingAudio {
    CommandId commandId{};
    std::optional<float> volume;
    std::optional<bool> muted;
    qint64 deadline{};
  };
  void applySnapshot(std::shared_ptr<const GraphSnapshot> snapshot);
  void applyStatus(SourceStatus status);
  void applyCommandResult(CommandResult result);
  void applyAudioResult(GlobalId nodeId, CommandResult result);
  void rebuildPresentation();
  void rebuildRouting();
  void updateSelection();
  void saveCard(const QString &key);
  void submitCreate(GlobalId outputPortId, GlobalId inputPortId, bool feedback,
    OperationIntent intent = OperationIntent::Normal);
  void submitDestroy(const GraphLink &link, OperationIntent intent = OperationIntent::Normal);
  void resolvePending();
  void resolveAudioPending();
  void completePending(const GraphLink &observed);
  void failPending(const QString &message);
  void setNotice(QString message);

  std::unique_ptr<GraphSource> source_;
  LayoutStore layoutStore_;
  StableListModel cards_;
  StableListModel links_;
  QVariantList anchors_;
  QVariantList cardRects_;
  QVariantList renderedLinks_;
  QVariantMap selected_;
  QVariantMap routing_;
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
  QString noticeText_;
  bool connected_{};
  bool feedbackConfirmation_{};
  std::optional<GlobalId> routeOutputPort_;
  std::optional<GlobalId> routeTargetPort_;
  QPointF routeCursor_;
  std::optional<PendingOperation> pending_;
  QVector<HistoryAction> history_;
  qsizetype historyCursor_{};
  CommandId nextCommandId_{1};
  QTimer commandTimer_;
  QTimer audioTimer_;
  QHash<GlobalId, PendingAudio> pendingAudio_;
  double canvasWidth_{1000.0};
  double canvasHeight_{650.0};
};

} // namespace wirerunner
