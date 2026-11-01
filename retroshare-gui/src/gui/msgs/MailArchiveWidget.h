/*
 * MailArchiveWidget.h
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

#include "MailArchiveStore.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QSplitter>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QDateTime>
#include <QPushButton>
#include <QHBoxLayout>

// Used only to extract text from exported HTML. Never resolve a resource,
// including file:, data:, qrc: or network URLs.
class MailArchiveTextDocument : public QTextDocument
{
protected:
    QVariant loadResource(int, const QUrl&) override { return QVariant(); }
public:
    static QString plainText(const QString& html)
    {
        MailArchiveTextDocument document;
        document.setHtml(html);
        return document.toPlainText();
    }
};

class MailArchiveWidget : public QWidget
{
public:
    explicit MailArchiveWidget(const QString& path, QWidget* parent = nullptr)
        : QWidget(parent), mPath(path)
    {
        auto* layout = new QVBoxLayout(this);
        auto* description = new QLabel(tr("Imported Archive — read-only mail history. "
                "Import using Settings → Node → Import location data. Attachments are not included."), this);
        description->setWordWrap(true);
        layout->addWidget(description);
        mSearch = new QLineEdit(this);
        mSearch->setPlaceholderText(tr("Search archived subjects, addresses and message text"));
        mSearch->setObjectName("archiveSearch");
        auto* searchLayout = new QHBoxLayout;
        searchLayout->addWidget(mSearch);
        auto* refresh = new QPushButton(tr("Refresh"), this);
        searchLayout->addWidget(refresh);
        layout->addLayout(searchLayout);
        connect(refresh, &QPushButton::clicked, this, [this]() { reload(); });
        mStatus = new QLabel(this);
        mStatus->setTextFormat(Qt::PlainText);
        layout->addWidget(mStatus);
        auto* splitter = new QSplitter(Qt::Vertical, this);
        mList = new QTreeWidget(splitter);
        mList->setObjectName("archiveMessages");
        mList->setRootIsDecorated(false);
        mList->setHeaderLabels({tr("Date"), tr("Original folder"), tr("From"), tr("Subject")});
        mList->header()->setStretchLastSection(true);
        mList->setSortingEnabled(true);
        mBody = new QPlainTextEdit(splitter);
        mBody->setObjectName("archiveBody");
        mBody->setReadOnly(true);
        mBody->setPlaceholderText(tr("Select an archived message to read it."));
        layout->addWidget(splitter);
        connect(mList, &QTreeWidget::currentItemChanged, this,
                [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
            mBody->clear();
            if(!item) return;
            const auto m = mMessages.at(item->data(0, Qt::UserRole).toInt()).toObject();
            // Plain text only: no HTML renderer, clickable URLs or downloads.
            mBody->setPlainText(tr("Subject: %1\nFrom: %2\nTo: %3\nRecipients: %4\nDate: %5\nOriginal folder: %6\n\n%7")
                    .arg(m["subject"].toString(), m["from"].toString(), m["to"].toString(),
                         m["recipients"].toString(), item->text(0), item->text(1), m["body"].toString()));
        });
        connect(mSearch, &QLineEdit::textChanged, this, [this]() { filter(); });
    }

    void reload()
    {
        mList->clear();
        mBody->clear();
        QString error;
        if(!MailArchiveStore::load(mPath, mMessages, error))
        { mMessages = QJsonArray(); mStatus->setText(error); return; }
        mList->setSortingEnabled(false);
        for(int i = 0; i < mMessages.size(); ++i)
        {
            const auto m = mMessages.at(i).toObject();
            const auto date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(m["date"].toDouble()));
            auto* item = new QTreeWidgetItem(mList, {date.toLocalTime().toString("yyyy-MM-dd HH:mm:ss"),
                    folderName(m["folder"].toString()), m["from"].toString(), m["subject"].toString()});
            item->setData(0, Qt::UserRole, i);
        }
        mList->setSortingEnabled(true);
        mList->sortItems(0, Qt::DescendingOrder);
        for(int i = 0; i < 3; ++i) mList->resizeColumnToContents(i);
        filter();
    }

private:
    static QString folderName(const QString& name)
    {
        if(name == "inbox") return tr("Inbox");
        if(name == "sent") return tr("Sent");
        if(name == "drafts") return tr("Drafts");
        if(name == "outbox") return tr("Outbox (not sent)");
        return tr("Trash");
    }
    void filter()
    {
        int visible = 0;
        const auto query = mSearch->text();
        for(int i = 0; i < mList->topLevelItemCount(); ++i)
        {
            auto* item = mList->topLevelItem(i);
            const auto m = mMessages.at(item->data(0, Qt::UserRole).toInt()).toObject();
            bool match = query.isEmpty();
            for(const auto* field : {"subject", "from", "to", "recipients", "body"})
                match = match || m[field].toString().contains(query, Qt::CaseInsensitive);
            item->setHidden(!match);
            if(match) ++visible;
        }
        if(mList->currentItem() && mList->currentItem()->isHidden())
        { mList->setCurrentItem(nullptr); mBody->clear(); }
        mStatus->setText(tr("%1 of %2 archived messages").arg(visible).arg(mMessages.size()));
    }
    QString mPath;
    QJsonArray mMessages;
    QLineEdit* mSearch;
    QLabel* mStatus;
    QTreeWidget* mList;
    QPlainTextEdit* mBody;
};
