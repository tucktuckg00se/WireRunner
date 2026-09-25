// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/StableListModel.hpp"

#include <QSet>

namespace wirerunner {

int StableListModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(items_.size());
}

QVariant StableListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) return {};
  if (role == KeyRole) return items_.at(index.row()).value(QStringLiteral("key"));
  if (role == ItemRole) return items_.at(index.row());
  return {};
}

QHash<int, QByteArray> StableListModel::roleNames() const {
  return {{KeyRole, "key"}, {ItemRole, "item"}};
}

QVariantMap StableListModel::get(int row) const {
  return row >= 0 && row < items_.size() ? items_.at(row) : QVariantMap{};
}

void StableListModel::setItems(QList<QVariantMap> desired) {
  QSet<QString> desiredKeys;
  for (const auto &item : desired) desiredKeys.insert(item.value(QStringLiteral("key")).toString());
  for (qsizetype row = items_.size(); row > 0; --row) {
    if (desiredKeys.contains(items_.at(row - 1).value(QStringLiteral("key")).toString())) continue;
    beginRemoveRows({}, static_cast<int>(row - 1), static_cast<int>(row - 1));
    items_.removeAt(row - 1);
    endRemoveRows();
    emit countChanged();
  }

  for (qsizetype target = 0; target < desired.size(); ++target) {
    const auto key = desired.at(target).value(QStringLiteral("key")).toString();
    qsizetype current = target;
    while (current < items_.size() && items_.at(current).value(QStringLiteral("key")).toString() != key) ++current;
    if (current == items_.size()) {
      beginInsertRows({}, static_cast<int>(target), static_cast<int>(target));
      items_.insert(target, desired.at(target));
      endInsertRows();
      emit countChanged();
      continue;
    }
    if (current != target) {
      const auto destination = current < target ? target + 1 : target;
      beginMoveRows({}, static_cast<int>(current), static_cast<int>(current), {}, static_cast<int>(destination));
      items_.move(current, target);
      endMoveRows();
    }
    if (items_.at(target) != desired.at(target)) {
      items_[target] = desired.at(target);
      emit dataChanged(index(static_cast<int>(target)), index(static_cast<int>(target)), {KeyRole, ItemRole});
    }
  }
}

} // namespace wirerunner
