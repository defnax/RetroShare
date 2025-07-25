/*******************************************************************************
 * gui/statistics/GxsTransportStatistics.cpp                                   *
 *                                                                             *
 * Copyright (c) 2011 Retroshare Team <retroshare.project@gmail.com>           *
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

#include <iostream>
#include <time.h>
#include <memory>

#include <QDateTime>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLayout>
#include <QMenu>
#include <QObject>
#include <QPainter>
#include <QStylePainter>
#include <QTimer>
#include <QTreeWidget>
#include <QWheelEvent>

#include <retroshare/rsgxstrans.h>
#include <retroshare/rspeers.h>
#include <retroshare/rsidentity.h>
#include <retroshare/rsgxstrans.h>

#include "GxsTransportStatistics.h"

#include "gui/Identity/IdDetailsDialog.h"
#include "gui/settings/rsharesettings.h"
#include "util/RsQtVersion.h"
#include "gui/common/UIStateHelper.h"
#include "util/misc.h"
#include "util/qtthreadsutils.h"
#include "gui/gxs/GxsIdLabel.h"
#include "gui/gxs/GxsIdDetails.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "gui/Identity/IdDialog.h"
#include "gui/MainWindow.h"
#include "gui/common/FilesDefs.h"
#include "util/DateTime.h"

#define COL_PENDING_ID                  0
#define COL_PENDING_DESTINATION         1
#define COL_PENDING_DATASTATUS          2
#define COL_PENDING_DATASIZE            3
#define COL_PENDING_DATAHASH            4
#define COL_PENDING_SEND                5
#define COL_PENDING_GROUP_ID            6
#define COL_PENDING_SENDTIME			7
#define COL_PENDING_DESTINATION_ID      8

#define COL_GROUP_GRP_ID                  0
#define COL_GROUP_PUBLISHTS               1
#define COL_GROUP_NUM_MSGS                2
#define COL_GROUP_SIZE_MSGS               3
#define COL_GROUP_SUBSCRIBED              4
#define COL_GROUP_POPULARITY              5
#define COL_GROUP_UNIQUE_ID               6
#define COL_GROUP_AUTHOR_ID               7

//static const int PARTIAL_VIEW_SIZE                           =  9 ;
//static const int MAX_TUNNEL_REQUESTS_DISPLAY                 = 10 ;
//static const int GXSTRANS_STATISTICS_DELAY_BETWEEN_GROUP_REQ = 30 ;	// never request more than every 30 secs.

#define GXSTRANS_GROUP_META  0x01
#define GXSTRANS_GROUP_DATA  0x02
#define GXSTRANS_GROUP_STAT  0x03
#define GXSTRANS_MSG_META    0x04

//#define DEBUG_GXSTRANS_STATS 1

GxsTransportStatistics::GxsTransportStatistics(QWidget *parent)
    : MainPage(parent)
{
	setupUi(this) ;
	
	mStateHelper = new UIStateHelper(this);
	mStateHelper->addWidget(GXSTRANS_GROUP_META, treeWidget);

	m_bProcessSettings = false;
    mLastGroupReqTS = 0 ;

	/* Set header resize modes and initial section sizes Uploads TreeView*/
	QHeaderView_setSectionResizeMode(treeWidget->header(), QHeaderView::ResizeToContents);
	QHeaderView_setSectionResizeMode(groupTreeWidget->header(), QHeaderView::ResizeToContents);

	connect(treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(CustomPopupMenu(QPoint)));
	connect(groupTreeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(CustomPopupMenuGroups(QPoint)));

	treeWidget->setColumnHidden(COL_PENDING_DESTINATION_ID,true);
	groupTreeWidget->setColumnHidden(COL_GROUP_AUTHOR_ID,true);

	// load settings
	processSettings(true);
	updateDisplay(true);
}

GxsTransportStatistics::~GxsTransportStatistics()
{

    // save settings
    processSettings(false);
}

void GxsTransportStatistics::processSettings(bool bLoad)
{
    m_bProcessSettings = true;

    Settings->beginGroup(QString("GxsTransportStatistics"));

    if (bLoad) {
        // load settings

        // state of splitter
        //splitter->restoreState(Settings->value("Splitter").toByteArray());
    } else {
        // save settings

        // state of splitter
        //Settings->setValue("Splitter", splitter->saveState());

    }

    Settings->endGroup();
    
    m_bProcessSettings = false;
}

void GxsTransportStatistics::CustomPopupMenu( QPoint )
{
	QMenu contextMnu( this );
	
	QTreeWidgetItem *item = treeWidget->currentItem();
	if (item) {
	contextMnu.addAction(FilesDefs::getIconFromQtResourcePath(":/images/info16.png"), tr("View details"), this, SLOT(personDetails()));

  }

	contextMnu.exec(QCursor::pos());
}

void GxsTransportStatistics::CustomPopupMenuGroups( QPoint )
{
	QMenu contextMnu( this );
	
	QTreeWidgetItem *item = groupTreeWidget->currentItem();
	if (item) {
	contextMnu.addAction(FilesDefs::getIconFromQtResourcePath(":/images/info16.png"), tr("View details"), this, SLOT(showAuthorInPeople()));

  }

	contextMnu.exec(QCursor::pos());
}

void GxsTransportStatistics::updateDisplay(bool)
{
	time_t now = time(NULL) ;
#ifdef DEBUG_GXSTRANS_STATS
	std::cerr << "GxsTransportStatistics::updateDisplay()" << std::endl;
#endif

	loadGroups();

	mLastGroupReqTS = now ;
}

QString GxsTransportStatistics::getPeerName(const RsPeerId &peer_id)
{
	static std::map<RsPeerId, QString> names ;

	std::map<RsPeerId,QString>::const_iterator it = names.find(peer_id) ;

	if( it != names.end())
		return it->second ;
	else
	{
		RsPeerDetails detail ;
		if(!rsPeers->getPeerDetails(peer_id,detail))
			return tr("Unknown Peer");

		return (names[peer_id] = QString::fromUtf8(detail.name.c_str())) ;
	}
}

static QString getStatusString(GxsTransSendStatus status)
{
    switch(status)
	{
    case GxsTransSendStatus::PENDING_PROCESSING         :  return QObject::tr("Processing") ;
	case GxsTransSendStatus::PENDING_PREFERRED_GROUP    :  return QObject::tr("Choosing group") ;
	case GxsTransSendStatus::PENDING_RECEIPT_CREATE     :  return QObject::tr("Creating receipt") ;
	case GxsTransSendStatus::PENDING_RECEIPT_SIGNATURE  :  return QObject::tr("Signing receipt") ;
	case GxsTransSendStatus::PENDING_SERIALIZATION      :  return QObject::tr("Serializing") ;
	case GxsTransSendStatus::PENDING_PAYLOAD_CREATE     :  return QObject::tr("Creating payload") ;
	case GxsTransSendStatus::PENDING_PAYLOAD_ENCRYPT    :  return QObject::tr("Encrypting payload") ;
	case GxsTransSendStatus::PENDING_PUBLISH            :  return QObject::tr("Publishing") ;
	case GxsTransSendStatus::PENDING_RECEIPT_RECEIVE    :  return QObject::tr("Waiting for receipt") ;
	case GxsTransSendStatus::RECEIPT_RECEIVED           :  return QObject::tr("Receipt received") ;
	case GxsTransSendStatus::FAILED_RECEIPT_SIGNATURE   :  return QObject::tr("Receipt signature failed") ;
	case GxsTransSendStatus::FAILED_ENCRYPTION          :  return QObject::tr("Encryption failed") ;
	case GxsTransSendStatus::UNKNOWN                    :
	default                                             :  return QObject::tr("Unknown") ;
	}
}

void GxsTransportStatistics::updateContent()
{
    RsGxsTrans::GxsTransStatistics transinfo ;

    rsGxsTrans->getDataStatistics(transinfo) ;

    // clear

    treeWidget->clear();
    //time_t now = time(NULL) ;
    
    // 1 - fill the table for pending packets
	
	time_t now = time(NULL) ;

    groupBox->setTitle(tr("Pending data items")+": " + QString::number(transinfo.outgoing_records.size()) );

    for(uint32_t i=0;i<transinfo.outgoing_records.size();++i)
    {
        const RsGxsTransOutgoingRecord& rec(transinfo.outgoing_records[i]) ;

        //QTreeWidgetItem *item = new QTreeWidgetItem();
		GxsIdRSTreeWidgetItem *item = new GxsIdRSTreeWidgetItem(NULL,GxsIdDetails::ICON_TYPE_AVATAR) ;
        treeWidget->addTopLevelItem(item);
        
        RsIdentityDetails details ;
        rsIdentity->getIdDetails(rec.recipient,details);
        QString nickname = QString::fromUtf8(details.mNickname.c_str());
        
        if(nickname.isEmpty())
          nickname = tr("Unknown");

	    item -> setId(rec.recipient,COL_PENDING_DESTINATION, false) ;
        item -> setData(COL_PENDING_ID,           Qt::DisplayRole, QString::number(rec.trans_id,16).rightJustified(8,'0'));
        item -> setData(COL_PENDING_DATASTATUS,   Qt::DisplayRole, getStatusString(rec.status));
        item -> setData(COL_PENDING_DATASIZE,     Qt::DisplayRole, misc::friendlyUnit(rec.data_size));
        item -> setData(COL_PENDING_DATAHASH,     Qt::DisplayRole, QString::fromStdString(rec.data_hash.toStdString()));
		item -> setData(COL_PENDING_SEND,         Qt::DisplayRole, DateTime::DateTimeFromTime_t(rec.send_TS).toString());
        item -> setData(COL_PENDING_GROUP_ID,     Qt::DisplayRole, QString::fromStdString(rec.group_id.toStdString()));
		item -> setData(COL_PENDING_DESTINATION_ID,  Qt::DisplayRole, QString::fromStdString(rec.recipient.toStdString()));
		item -> setData(COL_PENDING_SENDTIME,        Qt::DisplayRole, QString::number(now - rec.send_TS));

		item->setTextAlignment(COL_PENDING_DATASIZE, Qt::AlignRight	);
		item->setTextAlignment(COL_PENDING_SEND, Qt::AlignRight	);

    }

    // 2 - fill the table for pending group data

    // record openned groups

    std::set<RsGxsGroupId> openned_groups ;

	for(int i=0; i<groupTreeWidget->topLevelItemCount(); ++i)
		if( groupTreeWidget->isItemExpanded(groupTreeWidget->topLevelItem(i)) )
			openned_groups.insert(RsGxsGroupId(groupTreeWidget->topLevelItem(i)->data(COL_GROUP_GRP_ID, Qt::DisplayRole).toString().toStdString()));

	groupTreeWidget->clear();

    for(std::map<RsGxsGroupId,RsGxsTransGroupStatistics>::const_iterator it(mGroupStats.begin());it!=mGroupStats.end();++it)
    {
        const RsGxsTransGroupStatistics& stat(it->second) ;
		QTreeWidgetItem *item ;

		{
			QString unique_id = QString::fromStdString(stat.mGrpId.toStdString());

			QList<QTreeWidgetItem*> iteml = groupTreeWidget->findItems(unique_id,Qt::MatchExactly,COL_GROUP_UNIQUE_ID) ;

			if(iteml.empty())
				item = new QTreeWidgetItem;
			else
				item = *iteml.begin();
		}

        groupTreeWidget->addTopLevelItem(item);
		groupTreeWidget->setItemExpanded(item,openned_groups.find(it->first) != openned_groups.end());

		QString msg_time_string = (stat.last_publish_TS>0)?QString("(Last msg: %1)").arg(DateTime::DateTimeFromTime_t((uint)stat.last_publish_TS).toString()):"" ;

        item->setData(COL_GROUP_PUBLISHTS,  Qt::DisplayRole,  msg_time_string) ;
		item->setData(COL_GROUP_NUM_MSGS,  Qt::DisplayRole,  QString::number(stat.mNumMsgs) ) ;
        item->setData(COL_GROUP_GRP_ID,    Qt::DisplayRole,  QString::fromStdString(it->first.toStdString())) ;
        item->setData(COL_GROUP_SIZE_MSGS, Qt::DisplayRole,  QString::number(stat.mTotalSizeOfMsgs)) ;
        item->setData(COL_GROUP_SUBSCRIBED,Qt::DisplayRole,  stat.subscribed?tr("Yes"):tr("No")) ;
        item->setData(COL_GROUP_POPULARITY,Qt::DisplayRole,  QString::number(stat.popularity)) ;
        item->setData(COL_GROUP_UNIQUE_ID, Qt::DisplayRole,  QString::fromStdString(it->first.toStdString())) ;

        for(std::map<RsGxsMessageId,RsMsgMetaData>::const_iterator msgIt(stat.messages_metas.begin());msgIt!=stat.messages_metas.end();++msgIt)
        {
            const RsMsgMetaData& meta(msgIt->second);

            QTreeWidgetItem *sitem ;
			{
				QString unique_id = QString::fromStdString(meta.mMsgId.toStdString());

				QList<QTreeWidgetItem*> iteml = groupTreeWidget->findItems(unique_id,Qt::MatchExactly,COL_GROUP_UNIQUE_ID) ;

				if(iteml.empty())
					sitem = new QTreeWidgetItem(item) ;
				else
					sitem = *iteml.begin();
			}

            GxsIdLabel *label = new GxsIdLabel();
            label->setId(meta.mAuthorId) ;
            groupTreeWidget->setItemWidget(sitem,COL_GROUP_GRP_ID,label) ;
			
			RsIdentityDetails idDetails ;
			rsIdentity->getIdDetails(meta.mAuthorId,idDetails);
			
			QPixmap pixmap ;
			QDateTime qdatetime;
			qdatetime.setTime_t(meta.mPublishTs);

			if(idDetails.mAvatar.mSize == 0 || !GxsIdDetails::loadPixmapFromData(idDetails.mAvatar.mData, idDetails.mAvatar.mSize, pixmap,GxsIdDetails::SMALL))
				pixmap = GxsIdDetails::makeDefaultIcon(meta.mAuthorId,GxsIdDetails::SMALL);
				  
			sitem->setIcon(COL_GROUP_GRP_ID, QIcon(pixmap));

			sitem->setData(COL_GROUP_UNIQUE_ID, Qt::DisplayRole,QString::fromStdString(meta.mMsgId.toStdString()));
			sitem->setData(COL_GROUP_AUTHOR_ID, Qt::DisplayRole,  QString::fromStdString(meta.mAuthorId.toStdString())) ;
			sitem->setText(COL_GROUP_PUBLISHTS, DateTime::DateTimeFromTime_t(meta.mPublishTs).toString());
			sitem->setData(COL_GROUP_PUBLISHTS, Qt::UserRole, qdatetime);
        }
    }
}

void GxsTransportStatistics::personDetails()
{
    QTreeWidgetItem *item = treeWidget->currentItem();
    std::string id = item->text(COL_PENDING_DESTINATION_ID).toStdString();

    if (id.empty()) {
        return;
    }
    
    IdDetailsDialog *dialog = new IdDetailsDialog(RsGxsGroupId(id));
    dialog->show();
}

void GxsTransportStatistics::showAuthorInPeople()
{
    QTreeWidgetItem *item = groupTreeWidget->currentItem();
    std::string id = item->text(COL_GROUP_AUTHOR_ID).toStdString();

    if (id.empty()) {
        return;
    }

	/* window will destroy itself! */
	IdDialog *idDialog = dynamic_cast<IdDialog*>(MainWindow::getPage(MainWindow::People));

	if (!idDialog)
		return ;

	MainWindow::showWindow(MainWindow::People);
	idDialog->navigate(RsGxsId(id));
}

#ifdef TO_REMOVE
void GxsTransportStatistics::loadGroupMeta(const std::vector<RsGroupMetaData>& groupInfo)
{
	mStateHelper->setLoading(GXSTRANS_GROUP_META, false);

#ifdef DEBUG_GXSTRANS_STATS
	std::cerr << "GxsTransportStatisticsWidget::loadGroupMeta()";
	std::cerr << std::endl;
#endif

	mStateHelper->setActive(GXSTRANS_GROUP_META, true);

	std::set<RsGxsGroupId> existing_groups ;

	for(auto vit = groupInfo.begin(); vit != groupInfo.end(); ++vit)
	{
        existing_groups.insert(vit->mGroupId) ;

		/* Add Widget, and request Pages */
#ifdef DEBUG_GXSTRANS_STATS
		std::cerr << "GxsTransportStatisticsWidget::loadGroupMeta() GroupId: " << vit->mGroupId << " Group: " << vit->mGroupName << std::endl;
#endif

        loadGroupStats(vit->mGroupId) ;
        loadMsgMetas(vit->mGroupId) ;

        RsGxsTransGroupStatistics& s(mGroupStats[vit->mGroupId]);
        s.popularity = vit->mPop ;
        s.subscribed = IS_GROUP_SUBSCRIBED(vit->mSubscribeFlags) ;
		s.mGrpId = vit->mGroupId ;
	}

    // remove group stats for group that do not exist anymore

    for(std::map<RsGxsGroupId,RsGxsTransGroupStatistics>::iterator it(mGroupStats.begin());it!=mGroupStats.end();)
        if(existing_groups.find(it->first) == existing_groups.end())
			it = mGroupStats.erase(it);
		else
			++it;
}

void GxsTransportStatistics::loadGroupStats(const RsGxsGroupId& groupId)
{
#ifdef DEBUG_GXSTRANS_STATS
	std::cerr << "Loading group stats: " << stats.mGrpId << ", num msgs=" << stats.mNumMsgs << ", total size=" << stats.mTotalSizeOfMsgs << std::endl;
#endif

	RsThread::async([this]()
	{
        // 1 - get message data from p3GxsForums

#ifdef DEBUG_FORUMS
        std::cerr << "Retrieving post data for post " << mThreadId << std::endl;
#endif
		GxsGroupStatistic stats;
		rsGxsTrans->getGroupStatistic(groupId,stats);

        RsQThreadUtils::postToObject( [stats,this]()
		{
			/* Here it goes any code you want to be executed on the Qt Gui
			 * thread, for example to update the data model with new information
			 * after a blocking call to RetroShare API complete */

			dynamic_cast<GxsGroupStatistic&>(mGroupStats[stats.mGrpId]) = stats ;

			mStateHelper->setLoading(GXSTRANS_GROUP_STAT, false);

		}, this );

    });

}
#endif


void GxsTransportStatistics::loadGroups()
{
	mStateHelper->setLoading(GXSTRANS_GROUP_META, true);

	RsThread::async([this]()
	{
        // 1 - get message data from p3GxsForums

#ifdef DEBUG_FORUMS
        std::cerr << "Retrieving post data for post " << mThreadId << std::endl;
#endif
        auto stats = new std::map<RsGxsGroupId,RsGxsTransGroupStatistics>();

		if(!rsGxsTrans->getGroupStatistics(*stats))
		{
			RS_ERR("Cannot retrieve group statistics in GxsTransportStatistics");
            delete stats;
			return;
		}

        RsQThreadUtils::postToObject( [stats, this]()
		{
			/* Here it goes any code you want to be executed on the Qt Gui
			 * thread, for example to update the data model with new information
			 * after a blocking call to RetroShare API complete */

			// TODO: consider making mGroupStats an unique_ptr to avoid copying
			mGroupStats = *stats;
			updateContent();
			mStateHelper->setLoading(GXSTRANS_GROUP_META, false);

            delete stats;
        }, this );

    });
}


