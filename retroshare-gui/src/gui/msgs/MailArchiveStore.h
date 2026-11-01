/*
 * MailArchiveStore.h
 *
 * Copyright (C) 2026 RetroShare contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QCryptographicHash>
#include <QObject>
#include <cmath>

// GUI-owned historical mail. No message-service IDs are used as live mail IDs.
namespace MailArchiveStore
{
constexpr qint64 MaxBytes = 128 * 1024 * 1024;
constexpr int MaxMessages = 100000;

inline QString key(const QJsonObject& message)
{
    const QJsonArray identity{message["source"], message["id"]};
    return QString::fromLatin1(QCryptographicHash::hash(
            QJsonDocument(identity).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex());
}

inline bool validate(const QJsonArray& messages, QString& error)
{
    if(messages.size() > MaxMessages)
    { error = QObject::tr("The archive contains too many messages."); return false; }
    for(const auto& value : messages)
    {
        if(!value.isObject())
        { error = QObject::tr("Invalid archive message."); return false; }
        const auto m = value.toObject();
        for(const auto* field : {"source", "id", "subject", "from", "to", "recipients", "folder", "body"})
            if(!m[field].isString() || m[field].toString().size() >
                    (QString::fromLatin1(field) == "body" ? 16 * 1024 * 1024 : 64 * 1024))
            { error = QObject::tr("Invalid or oversized archive field."); return false; }
        const double date = m["date"].toDouble(-1);
        if(m["source"].toString().isEmpty() || m["id"].toString().isEmpty()
                || !m["date"].isDouble() || !std::isfinite(date)
                || date < 0 || date > 253402300799.0 || std::floor(date) != date
                || !QStringList{"inbox", "sent", "drafts", "outbox", "trash"}.contains(m["folder"].toString()))
        { error = QObject::tr("Invalid archive metadata."); return false; }
    }
    if(QJsonDocument(messages).toJson(QJsonDocument::Compact).size() > MaxBytes)
    { error = QObject::tr("The archive exceeds 128 MiB."); return false; }
    return true;
}

inline bool load(const QString& path, QJsonArray& messages, QString& error)
{
    messages = QJsonArray();
    if(!QFile::exists(path)) return true;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly) || file.size() > MaxBytes)
    { error = QObject::tr("Cannot read the local mail archive."); return false; }
    const auto bytes = file.read(MaxBytes + 1);
    if(file.error() != QFile::NoError || !file.atEnd() || bytes.size() > MaxBytes)
    { error = QObject::tr("Cannot read the local mail archive."); return false; }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(bytes, &parseError);
    const auto root = doc.object();
    if(parseError.error != QJsonParseError::NoError || !doc.isObject()
            || root["format"].toString() != "RetroShareImportedMail"
            || root["version"].toDouble() != 1 || !root["messages"].isArray())
    { error = QObject::tr("The local mail archive is damaged or unsupported."); return false; }
    messages = root["messages"].toArray();
    return validate(messages, error);
}

inline bool merge(const QString& path, const QJsonArray& incoming, bool dryRun,
                  int& added, int& skipped, QString& error)
{
    added = skipped = 0;
    if(!validate(incoming, error)) return false;
    QJsonArray messages;
    if(!load(path, messages, error)) return false;
    QSet<QString> known;
    for(const auto& value : messages) known.insert(key(value.toObject()));
    for(const auto& value : incoming)
    {
        const auto id = key(value.toObject());
        if(known.contains(id)) { ++skipped; continue; }
        known.insert(id);
        messages.append(value);
        ++added;
    }
    if(!validate(messages, error)) return false;
    const QJsonObject root{{"format", "RetroShareImportedMail"}, {"version", 1}, {"messages", messages}};
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if(bytes.size() > MaxBytes)
    { error = QObject::tr("The archive exceeds 128 MiB."); return false; }
    if(dryRun || !added) return true;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))
    { error = QObject::tr("Cannot write the local mail archive."); return false; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if(file.write(bytes) != bytes.size() || !file.commit())
    { error = QObject::tr("Could not save the local mail archive."); return false; }
    return true;
}
}
