// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation/LayoutStore.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace wirerunner {

LayoutStore::LayoutStore(QString path) : path_(std::move(path)) {
  if (path_.isEmpty()) {
    path_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
      + QStringLiteral("/layout.json");
  }
  load();
}

void LayoutStore::load() {
  QFile file(path_);
  if (!file.open(QIODevice::ReadOnly)) return;
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) return;
  const auto root = document.object();
  if (root.value(QStringLiteral("version")).toInt() != 1) return;
  const auto remotes = root.value(QStringLiteral("remotes")).toObject();
  for (auto remote = remotes.begin(); remote != remotes.end(); ++remote) {
    QHash<QString, CardLayout> cards;
    const auto values = remote.value().toObject().value(QStringLiteral("cards")).toObject();
    for (auto value = values.begin(); value != values.end(); ++value) {
      const auto object = value.value().toObject();
      if (!object.value(QStringLiteral("x")).isDouble() || !object.value(QStringLiteral("y")).isDouble()) continue;
      cards.insert(value.key(), {{object.value(QStringLiteral("x")).toDouble(),
        object.value(QStringLiteral("y")).toDouble()},
        object.value(QStringLiteral("expanded")).toBool(false)});
    }
    remotes_.insert(remote.key(), std::move(cards));
  }
}

std::optional<CardLayout> LayoutStore::card(const QString &remote, const QString &key) const {
  const auto remoteValues = remotes_.constFind(remote);
  if (remoteValues == remotes_.cend()) return std::nullopt;
  const auto value = remoteValues->constFind(key);
  if (value == remoteValues->cend()) return std::nullopt;
  return *value;
}

void LayoutStore::setCard(const QString &remote, const QString &key, CardLayout value) {
  remotes_[remote].insert(key, value);
}

bool LayoutStore::save(QString *errorMessage) const {
  QJsonObject remotes;
  for (auto remote = remotes_.cbegin(); remote != remotes_.cend(); ++remote) {
    QJsonObject cards;
    for (auto card = remote->cbegin(); card != remote->cend(); ++card) {
      cards.insert(card.key(), QJsonObject{{QStringLiteral("x"), card->position.x()},
        {QStringLiteral("y"), card->position.y()},
        {QStringLiteral("expanded"), card->expanded}});
    }
    remotes.insert(remote.key(), QJsonObject{{QStringLiteral("cards"), cards}});
  }
  QDir().mkpath(QFileInfo(path_).absolutePath());
  QSaveFile file(path_);
  if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(QJsonObject{
      {QStringLiteral("version"), 1}, {QStringLiteral("remotes"), remotes}}).toJson()) < 0 || !file.commit()) {
    if (errorMessage) *errorMessage = file.errorString();
    return false;
  }
  return true;
}

} // namespace wirerunner
