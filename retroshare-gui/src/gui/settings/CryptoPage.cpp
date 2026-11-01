/*******************************************************************************
 * gui/settings/CryptoPage.cpp                                                 *
 *                                                                             *
 * Copyright 2006, Retroshare Team <retroshare.project@gmail.com>              *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Affero General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Affero General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Affero General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#include <QDate>
#include <QMessageBox>
#include <QClipboard>
#include <QFile>
#include <QTextStream>
#include <QTextCodec>
#include <QTemporaryFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QDomDocument>
#include <retroshare/rsmail.h>
#include "gui/msgs/MailArchiveWidget.h"

#include "rshare.h"
#include "CryptoPage.h"
#include "util/misc.h"
#include "util/DateTime.h"
#include "retroshare/rsinit.h"
#include <gui/RetroShareLink.h>
#include <gui/connect/ConfCertDialog.h>
#include <gui/profile/ProfileManager.h>
#include <gui/statistics/StatisticsWindow.h>
#include <gui/common/NewFriendList.h>

#include <retroshare/rspeers.h> //for rsPeers variable
#include <retroshare/rsdisc.h> //for rsPeers variable

/** Constructor */
CryptoPage::CryptoPage(QWidget * parent, Qt::WindowFlags flags)
    : ConfigPage(parent, flags)
{
	/* Invoke the Qt Designer generated object setup routine */
	ui.setupUi(this);

	ui._shortFormat_CB->setChecked(true);

	//  connect(ui.copykeyButton, SIGNAL(clicked()), this, SLOT(copyPublicKey()));
	connect(ui.saveButton, SIGNAL(clicked()), this, SLOT(fileSaveAs()));
	connect(ui._includeSignatures_CB, SIGNAL(toggled(bool)), this, SLOT(load()));
	connect(ui._shortFormat_CB, SIGNAL(toggled(bool)), this, SLOT(load()));
	connect(ui._includeAllIPs_CB, SIGNAL(toggled(bool)), this, SLOT(load()));
	connect(ui._copyLink_PB, SIGNAL(clicked()), this, SLOT(copyRSLink()));
	connect(ui.showStats_PB, SIGNAL(clicked()), this, SLOT(showStats()));

	// hide profile manager as it causes bugs when generating a new profile.
	//ui.profile_Button->hide() ;

	//connect(ui.exportprofile,SIGNAL(clicked()), this, SLOT(profilemanager()));
	connect(ui.exportprofile,SIGNAL(clicked()), this, SLOT(exportProfile()));
	connect(ui.exportfriendslist,SIGNAL(clicked()), this, SLOT(exportFriendsList()) );
    connect(ui.exportLocationData, &QPushButton::clicked, this, &CryptoPage::exportLocationData);
    connect(ui.importLocationData, &QPushButton::clicked, this, &CryptoPage::importLocationData);

    // Remove this because it duplicates functionality of the HomePage.
    ui.retroshareId_LB->hide();
    ui.retroshareId_content_LB->hide();
    ui.stackPageCertificate->hide();

	ui.onlinesince->setText(DateTime::formatDateTime(RsApplication::startupTime()));
}

#ifdef UNUSED_CODE
void CryptoPage::profilemanager()
{
    ProfileManager().exec();
}
#endif

void CryptoPage::exportProfile()
{
    RsPgpId gpgId(rsPeers->getGPGOwnId());

    QString fname = QFileDialog::getSaveFileName(this, tr("Export Identity"), "", tr("RetroShare Identity files (*.asc)"));

    if (fname.isNull())
        return;

    if (fname.right(4).toUpper() != ".ASC") fname += ".asc";

    if (RsAccounts::ExportIdentity(fname.toUtf8().constData(), gpgId))
        QMessageBox::information(this, tr("Identity saved"), tr("Your identity was successfully saved\nIt is encrypted\n\nYou can now copy it to another computer\nand use the import button to load it"));
    else
        QMessageBox::information(this, tr("Identity not saved"), tr("Your identity was not saved. An error occurred."));
}

void CryptoPage::exportLocationData()
{
    if(QMessageBox::question(this, tr("Export location data"),
            tr("Export friends, groups and all mail for another location of this identity?\n\n"
               "The archive is not encrypted and contains private messages and contacts. "
               "Keep it in a secure place. Attachment files, private identity keys and chat history "
               "are not included. Mail is imported into a separate read-only Imported Archive folder, "
               "with plain-text bodies and no sending or replying.")) != QMessageBox::Yes)
        return;
    QString path = QFileDialog::getSaveFileName(this, tr("Export location data"),
            QString(), tr("RetroShare location archive (*.rslocation)"));
    if(path.isEmpty()) return;
    if(!path.endsWith(".rslocation", Qt::CaseInsensitive)) path += ".rslocation";
    auto fail = [this]() { QMessageBox::warning(this, tr("Export failed"),
            tr("Could not export the location data. Check disk space and file permissions. "
               "The mail archive must be smaller than 128 MiB.")); };
    QTemporaryFile friends;
    if(!friends.open()) { fail(); return; }
    QString friendsPath = friends.fileName();
    friends.close();
    NewFriendList friendList;
    if(!friendList.exportFriendlist(friendsPath) || !friends.open()) { fail(); return; }
    QByteArray friendsXml = friends.readAll();
    friendsXml.replace("<!DOCTYPE FriendListWithGroups>", "");
    if(friends.error() != QFile::NoError || friendsXml.size() > 16 * 1024 * 1024)
    { fail(); return; }
    QJsonArray mail;
    QString error;
    const QString localPath = QString::fromUtf8(RsAccounts::AccountDirectory().c_str()) + "/ImportedMailArchive.json";
    if(!MailArchiveStore::load(localPath, mail, error)) { fail(); return; }
    qint64 mailSize = QJsonDocument(mail).toJson(QJsonDocument::Compact).size();
    QSet<QString> known;
    for(const auto& value : mail) known.insert(MailArchiveStore::key(value.toObject()));
    const QString source = QString::fromStdString(rsPeers->getOwnId().toStdString());
    const std::pair<Rs::Mail::BoxName, const char*> boxes[] = {
        {Rs::Mail::BoxName::BOX_INBOX, "inbox"}, {Rs::Mail::BoxName::BOX_SENT, "sent"},
        {Rs::Mail::BoxName::BOX_DRAFTS, "drafts"}, {Rs::Mail::BoxName::BOX_TRASH, "trash"},
        {Rs::Mail::BoxName::BOX_OUTBOX, "outbox"}};
    for(const auto& box : boxes)
    {
        std::list<Rs::Mail::MsgInfoSummary> summaries;
        if(!rsMail->getMessageSummaries(box.first, summaries)) { fail(); return; }
        for(const auto& summary : summaries)
        {
            Rs::Mail::MessageInfo info;
            if(!rsMail->getMessage(summary.msgId, info)) { fail(); return; }
            QJsonObject entry;
            entry["source"] = source;
            entry["id"] = QString::fromStdString(summary.msgId);
            if(known.contains(MailArchiveStore::key(entry))) continue;
            entry["folder"] = QString::fromLatin1(box.second);
            entry["date"] = static_cast<double>(summary.ts);
            entry["subject"] = QString::fromUtf8(info.title.c_str());
            entry["from"] = QString::fromStdString(info.from.toStdString());
            entry["to"] = QString::fromStdString(info.to.toStdString());
            QStringList recipients;
            for(const auto& address : info.destinations)
            {
                const QString mode = address.mode() == Rs::Mail::MsgAddress::MSG_ADDRESS_MODE_CC ? "CC: " :
                        address.mode() == Rs::Mail::MsgAddress::MSG_ADDRESS_MODE_BCC ? "BCC: " : "To: ";
                recipients.append(mode + QString::fromStdString(address.toStdString()));
            }
            entry["recipients"] = recipients.join("; ");
            if(info.msg.size() > 16 * 1024 * 1024) { fail(); return; }
            entry["body"] = MailArchiveTextDocument::plainText(QString::fromUtf8(info.msg.c_str()));
            mailSize += QJsonDocument(entry).toJson(QJsonDocument::Compact).size() + 1;
            if(mailSize > MailArchiveStore::MaxBytes) { fail(); return; }
            known.insert(MailArchiveStore::key(entry));
            mail.append(entry);
            if(mail.size() > MailArchiveStore::MaxMessages) { fail(); return; }
        }
    }
    if(!MailArchiveStore::validate(mail, error)) { fail(); return; }
    QJsonObject archive;
    archive["format"] = "RetroShareLocation";
    archive["version"] = 2;
    archive["identity"] = QString::fromStdString(rsPeers->getGPGOwnId().toStdString());
    archive["sourceLocation"] = QString::fromStdString(rsPeers->getOwnId().toStdString());
    archive["friends"] = QString::fromUtf8(friendsXml);
    archive["mail"] = mail;
    archive["mailSha256"] = QString::fromLatin1(
            QCryptographicHash::hash(QJsonDocument(mail).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    const QByteArray bytes = QJsonDocument(archive).toJson(QJsonDocument::Compact);
    if(bytes.size() > 192 * 1024 * 1024) { fail(); return; }
    QSaveFile output(path);
    if(!output.open(QIODevice::WriteOnly)) { fail(); return; }
    output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if(output.write(bytes) != bytes.size() || !output.commit()) { fail(); return; }
    QMessageBox::information(this, tr("Location data exported"),
            tr("Friends and mail were exported. On the new location, sign in with the same "
               "PGP identity and choose Import location data. Mail will appear in Mail → Imported Archive."));
}

void CryptoPage::importLocationData()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import location data"),
            QString(), tr("RetroShare location archive (*.rslocation)"));
    if(path.isEmpty()) return;
    auto invalid = [this]() { QMessageBox::warning(this, tr("Import failed"),
            tr("The archive is unreadable, damaged, too large, or uses an unsupported format. "
               "No data was imported.")); };
    QFile input(path);
    if(!input.open(QIODevice::ReadOnly) || input.size() > 192 * 1024 * 1024)
    { invalid(); return; }
    const QByteArray bytes = input.read(192 * 1024 * 1024 + 1);
    if(input.error() != QFile::NoError || !input.atEnd()
            || bytes.size() > 192 * 1024 * 1024) { invalid(); return; }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if(error.error != QJsonParseError::NoError || !document.isObject())
    { invalid(); return; }
    const QJsonObject archive = document.object();
    if(archive["format"].toString() != "RetroShareLocation"
            || archive["version"].toDouble() != 2
            || !archive["friends"].isString() || !archive["mail"].isArray())
    { invalid(); return; }
    if(archive["identity"].toString()
            != QString::fromStdString(rsPeers->getGPGOwnId().toStdString()))
    {
        QMessageBox::warning(this, tr("Different identity"),
                tr("This archive belongs to a different PGP identity. Sign in with the "
                   "identity used to create it before importing."));
        return;
    }
    const QJsonArray mail = archive["mail"].toArray();
    if(QString::fromLatin1(QCryptographicHash::hash(
            QJsonDocument(mail).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex())
            != archive["mailSha256"].toString()) { invalid(); return; }
    const QString localPath = QString::fromUtf8(RsAccounts::AccountDirectory().c_str()) + "/ImportedMailArchive.json";
    int imported = 0, skipped = 0;
    QString archiveError;
    if(!MailArchiveStore::merge(localPath, mail, true, imported, skipped, archiveError))
    { QMessageBox::warning(this, tr("Import failed"), archiveError); return; }
    const QByteArray friendsXml = archive["friends"].toString().toUtf8();
    QDomDocument friendsDoc;
    if(friendsXml.size() > 16 * 1024 * 1024 || friendsXml.contains("<!DOCTYPE")
            || !friendsDoc.setContent(friendsXml)) { invalid(); return; }
    const QDomElement root = friendsDoc.documentElement();
    if(root.tagName() != "root" || root.elementsByTagName("pgpIDs").size() != 1
            || root.elementsByTagName("groups").size() != 1) { invalid(); return; }
    const QDomNodeList certificates = root.elementsByTagName("sslID");
    for(int i = 0; i < certificates.size(); ++i)
    {
        RsPeerDetails details;
        uint32_t certError = 0;
        if(!rsPeers->loadDetailsFromStringCert(certificates.at(i).toElement()
                .attribute("certificate").toStdString(), details, certError))
        { invalid(); return; }
    }
    if(QMessageBox::question(this, tr("Import location data"),
            tr("Import %1 friend location certificates and %2 mail messages? "
               "%3 duplicate messages will be skipped.\n\n"
               "Friends, groups and their exported permissions will be merged. "
               "Mail is added to the separate read-only Imported Archive folder. Nothing will be sent. "
               "Archived mail is stored unencrypted in this location. "
               "Attachment files and private keys are not included. "
               "Only import an archive you trust.")
            .arg(certificates.size()).arg(imported).arg(skipped)) != QMessageBox::Yes)
        return;
    QTemporaryFile friends;
    if(!friends.open() || friends.write(friendsXml) != friendsXml.size() || !friends.flush())
    { invalid(); return; }
    QString friendsPath = friends.fileName();
    friends.close();
    NewFriendList friendList;
    bool peerErrors = false, groupErrors = false;
    if(!MailArchiveStore::merge(localPath, mail, false, imported, skipped, archiveError))
    { QMessageBox::warning(this, tr("Import failed"), archiveError); return; }
    const bool friendsOk = friendList.importFriendlist(friendsPath, peerErrors, groupErrors);
    if(!friendsOk)
        QMessageBox::warning(this, tr("Import incomplete"),
                tr("Mail was saved in Imported Archive, but some friends or groups could not be imported. "
                   "You can retry; archived mail will be skipped."));
    else
        QMessageBox::information(this, tr("Location data imported"),
                tr("Imported %1 messages; skipped %2 duplicates. Friends and groups were merged.\n\n"
                   "Open Mail → Imported Archive to read and search the messages. "
                   "Archived messages cannot be sent or replied to. Attachments are not included.")
                .arg(imported).arg(skipped));
}

void CryptoPage::showEvent ( QShowEvent * /*event*/ )
{
    RsPeerDetails detail;
    if (rsPeers->getPeerDetails(rsPeers->getOwnId(),detail))
    {
        ui.name->setText(QString::fromUtf8(detail.name.c_str()));
        ui.country->setText(QString::fromUtf8(detail.location.c_str()));

        ui.peerid->setText(QString::fromStdString(detail.id.toStdString()));
        ui.pgpid->setText(QString::fromStdString(detail.gpg_id.toStdString()));
        ui.pgpfingerprint->setText(misc::fingerPrintStyleSplit(QString::fromStdString(detail.fpr.toStdString())));

        std::string invite ;
        rsPeers->getShortInvite(invite,rsPeers->getOwnId(),RetroshareInviteFlags::RADIX_FORMAT | RsPeers::defaultCertificateFlags);
        ui.retroshareId_content_LB->setText(QString::fromUtf8(invite.c_str()));
		
        /* [Modified] Show both RetroShare and libretroshare versions */
        /* We rename the label "Software Version:" to "RetroShare version:" and add the lbretroshare line */
        /* Using newlines (\n) allows us to display both without modifying the .ui grid layout */
        ui.label_3->setText(tr("RetroShare version:") + "\n" + tr("libretroshare version:"));

        QString versionText = RsApplication::retroshareVersion(true);
        versionText += "\n";
        versionText += QString::fromUtf8(RsInit::libRetroShareVersion());
        ui.version->setText(versionText);

        std::list<RsPgpId> ids;
        ids.clear();
        rsPeers->getGPGAcceptedList(ids);
        int friends = ids.size();

        ui.friendsEdit->setText(QString::number(friends));
		
		QString string ;
		string = rsFiles->getPartialsDirectory().c_str();
		QString datadir = string;
			if(datadir.contains("Partials"))
			{
				datadir.replace("Partials","");
			}
				ui.labelpath->setText(datadir);
    }
	 load() ;
}

CryptoPage::~CryptoPage()
{
}

/** Loads the settings for this page */
void
CryptoPage::load()
{
    std::string cert ;
    RetroshareInviteFlags flags = RetroshareInviteFlags::DNS | RetroshareInviteFlags::CURRENT_LOCAL_IP | RetroshareInviteFlags::CURRENT_EXTERNAL_IP;

    if(ui._shortFormat_CB->isChecked())
    {
        if(ui._includeAllIPs_CB->isChecked())
            flags |= RetroshareInviteFlags::FULL_IP_HISTORY;

        rsPeers->getShortInvite(cert,rsPeers->getOwnId(), RetroshareInviteFlags::RADIX_FORMAT | flags);
    }
	else
    {
        if(ui._includeSignatures_CB->isChecked())
            flags |= RetroshareInviteFlags::PGP_SIGNATURES;

        cert = rsPeers->GetRetroshareInvite( rsPeers->getOwnId(), flags);
    }

	ui.certplainTextEdit->setPlainText( QString::fromUtf8( cert.c_str() ) );

    RsPeerDetails detail;
    rsPeers->getPeerDetails(rsPeers->getOwnId(),detail);

    ui.certplainTextEdit->setToolTip(ConfCertDialog::getCertificateDescription(detail, ui._includeSignatures_CB->isChecked(), ui._shortFormat_CB->isChecked(), flags));
}

void
CryptoPage::copyRSLink()
{
	RsPeerId ownId = rsPeers->getOwnId() ;
	RetroShareLink link = RetroShareLink::createCertificate(ownId);

	if( link.valid() )
	{
		QList<RetroShareLink> urls ;

		urls.push_back(link) ;
		RSLinkClipboard::copyLinks(urls) ;
		QMessageBox::information(this,
				"RetroShare",
				tr("A RetroShare link with your Public Key is copied to Clipboard, paste and send it to your"
					" friend via email or some other way"));
	}
	else
		QMessageBox::warning(this,
				tr("Error"),
				tr("Your certificate could not be parsed correctly. Please contact the developers."));
}
void
CryptoPage::copyPublicKey()
{
    QMessageBox::information(this,
                             tr("RetroShare"),
                             tr("Your Public Key is copied to Clipboard, paste and send it to your"
                                " friend via email or some other way"));
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(ui.certplainTextEdit->toPlainText());
}

bool CryptoPage::fileSave()
{
    if (fileName.isEmpty())
        return fileSaveAs();

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly))
        return false;
    QTextStream ts(&file);
#if QT_VERSION >= QT_VERSION_CHECK (6, 0, 0)
    ts.setEncoding(QStringConverter::Utf8);
#else
    ts.setCodec(QTextCodec::codecForName("UTF-8"));
#endif
    ts << ui.certplainTextEdit->document()->toPlainText();
    return true;
}

bool CryptoPage::fileSaveAs()
{
    QString fn;
    if (misc::getSaveFileName(this, RshareSettings::LASTDIR_CERT, tr("Save as..."), tr("RetroShare Certificate (*.rsc );;All Files (*)"), fn)) {
        fileName = fn;
        return fileSave();
    }
    return false;
}

void CryptoPage::showStats()
{
    StatisticsWindow::showYourself();
}

void CryptoPage::exportFriendsList()
{
	NewFriendList().exportFriendlistClicked();
}
