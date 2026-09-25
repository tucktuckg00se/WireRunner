// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <optional>

namespace wirerunner {

struct CardLayout {
  QPointF position;
  bool expanded{};
};

class LayoutStore final {
public:
  explicit LayoutStore(QString path = {});
  std::optional<CardLayout> card(const QString &remote, const QString &key) const;
  void setCard(const QString &remote, const QString &key, CardLayout value);
  bool save(QString *errorMessage = nullptr) const;
  QString path() const { return path_; }

private:
  void load();
  QString path_;
  QHash<QString, QHash<QString, CardLayout>> remotes_;
};

} // namespace wirerunner
