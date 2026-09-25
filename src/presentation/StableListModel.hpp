// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

namespace wirerunner {

class StableListModel final : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
  enum Role { KeyRole = Qt::UserRole + 1, ItemRole };
  explicit StableListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  int count() const { return static_cast<int>(items_.size()); }
  Q_INVOKABLE QVariantMap get(int row) const;
  void setItems(QList<QVariantMap> items);

signals:
  void countChanged();

private:
  QList<QVariantMap> items_;
};

} // namespace wirerunner
