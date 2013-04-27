/*
* Copyright (C) 2008-2013 The Communi Project
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "sessiontreewidget.h"
#include "sessiontreedelegate.h"
#include "sessiontreeitem.h"
#include "messageview.h"
#include "menufactory.h"
#include "sharedtimer.h"
#include "session.h"
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QTimer>

SessionTreeWidget::SessionTreeWidget(QWidget* parent) : QTreeWidget(parent)
{
    setAnimated(true);
    setColumnCount(2);
    setIndentation(0);
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setFrameStyle(QFrame::NoFrame);

    header()->setStretchLastSection(false);
    header()->setResizeMode(0, QHeaderView::Stretch);
    header()->setResizeMode(1, QHeaderView::Fixed);
    header()->resizeSection(1, 22);

    setItemDelegate(new SessionTreeDelegate(this));

    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDragDropMode(InternalMove);

    d.dropParent = 0;
    d.menuFactory = 0;
    d.currentRestored = false;
    d.itemResetBlocked = false;

    d.colors[Active] = palette().color(QPalette::WindowText);
    d.colors[Inactive] = palette().color(QPalette::Disabled, QPalette::Highlight);
    d.colors[Highlight] = palette().color(QPalette::Highlight);

    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(onItemExpanded(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
            this, SLOT(onItemCollapsed(QTreeWidgetItem*)));
    connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
            this, SLOT(onCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

    d.prevShortcut = new QShortcut(this);
    connect(d.prevShortcut, SIGNAL(activated()), this, SLOT(moveToPrevItem()));

    d.nextShortcut = new QShortcut(this);
    connect(d.nextShortcut, SIGNAL(activated()), this, SLOT(moveToNextItem()));

    d.prevActiveShortcut = new QShortcut(this);
    connect(d.prevActiveShortcut, SIGNAL(activated()), this, SLOT(moveToPrevActiveItem()));

    d.nextActiveShortcut = new QShortcut(this);
    connect(d.nextActiveShortcut, SIGNAL(activated()), this, SLOT(moveToNextActiveItem()));

    d.expandShortcut = new QShortcut(this);
    connect(d.expandShortcut, SIGNAL(activated()), this, SLOT(expandCurrentSession()));

    d.collapseShortcut = new QShortcut(this);
    connect(d.collapseShortcut, SIGNAL(activated()), this, SLOT(collapseCurrentSession()));

    d.mostActiveShortcut = new QShortcut(this);
    connect(d.mostActiveShortcut, SIGNAL(activated()), this, SLOT(moveToMostActiveItem()));

    applySettings(d.settings);
}

QSize SessionTreeWidget::sizeHint() const
{
    return QSize(20 * fontMetrics().width('#'), QTreeWidget::sizeHint().height());
}

QByteArray SessionTreeWidget::saveState() const
{
    QByteArray state;
    QDataStream out(&state, QIODevice::WriteOnly);

    QVariantHash hash;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem* parent = topLevelItem(i);
        QStringList receivers;
        for (int j = 0; j < parent->childCount(); ++j)
            receivers += parent->child(j)->text(0);
        hash.insert(parent->text(0), receivers);
    }

    if (QTreeWidgetItem* item = currentItem()) {
        hash.insert("_currentText_", currentItem()->text(0));
        QTreeWidgetItem* parent = item->parent();
        if (!parent)
            parent = invisibleRootItem();
        hash.insert("_currentIndex_", parent->indexOfChild(item));
        hash.insert("_currentParent_", parent->text(0));
    }
    out << hash;
    return state;
}

void SessionTreeWidget::restoreState(const QByteArray& state)
{
    QVariantHash hash;
    QDataStream in(state);
    in >> hash;

    for (int i = 0; i < topLevelItemCount(); ++i) {
        SessionTreeItem* item = static_cast<SessionTreeItem*>(topLevelItem(i));
        item->d.sortOrder = hash.value(item->text(0)).toStringList();
        item->sortChildren(0, Qt::AscendingOrder);
    }

    if (!d.currentRestored && hash.contains("_currentText_")) {
        QList<QTreeWidgetItem*> candidates = findItems(hash.value("_currentText_").toString(), Qt::MatchFixedString | Qt::MatchCaseSensitive | Qt::MatchRecursive);
        foreach (QTreeWidgetItem* candidate, candidates) {
            QTreeWidgetItem* parent = candidate->parent();
            if (!parent)
                parent = invisibleRootItem();
            if (parent->indexOfChild(candidate) == hash.value("_currentIndex_").toInt()
                    && parent->text(0) == hash.value("_currentParent_").toString()) {
                setCurrentItem(candidate);
                d.currentRestored = true;
                break;
            }
        }
    }
}

MenuFactory* SessionTreeWidget::menuFactory() const
{
    if (!d.menuFactory) {
        SessionTreeWidget* that = const_cast<SessionTreeWidget*>(this);
        that->d.menuFactory = new MenuFactory(that);
    }
    return d.menuFactory;
}

void SessionTreeWidget::setMenuFactory(MenuFactory* factory)
{
    if (d.menuFactory && d.menuFactory->parent() == this)
        delete d.menuFactory;
    d.menuFactory = factory;
}

QColor SessionTreeWidget::statusColor(SessionTreeWidget::ItemStatus status) const
{
    return d.colors.value(status);
}

void SessionTreeWidget::setStatusColor(SessionTreeWidget::ItemStatus status, const QColor& color)
{
    d.colors[status] = color;
}

QColor SessionTreeWidget::currentBadgeColor() const
{
    if (!d.highlightColor.isValid() || d.highlightColor != d.colors.value(Active))
        return d.colors.value(Highlight).lighter(125);
    return qApp->palette().color(QPalette::Dark);
}

QColor SessionTreeWidget::currentHighlightColor() const
{
    if (!d.highlightColor.isValid())
        return d.colors.value(Highlight);
    return d.highlightColor;
}

SessionTreeItem* SessionTreeWidget::viewItem(MessageView* view) const
{
    return d.viewItems.value(view);
}

SessionTreeItem* SessionTreeWidget::sessionItem(Session* session) const
{
    return d.sessionItems.value(session);
}

bool SessionTreeWidget::hasRestoredCurrent() const
{
    return d.currentRestored;
}

ViewInfos SessionTreeWidget::viewInfos(Session* session) const
{
    ViewInfos views;
    SessionTreeItem* item = d.sessionItems.value(session);
    if (item) {
        for (int i = 0; i < item->childCount(); ++i) {
            SessionTreeItem* child = static_cast<SessionTreeItem*>(item->child(i));
            ViewInfo view;
            view.type = child->view()->viewType();
            view.name = child->view()->receiver();
            view.active = child->view()->isActive();
            views += view;
        }
    }
    return views;
}

void SessionTreeWidget::addView(MessageView* view)
{
    SessionTreeItem* item = 0;
    if (view->viewType() == ViewInfo::Server) {
        item = new SessionTreeItem(view, this);
        Session* session = view->session();
        connect(session, SIGNAL(nameChanged(QString)), this, SLOT(updateSession()));
        connect(session, SIGNAL(networkChanged(QString)), this, SLOT(updateSession()));
        d.sessionItems.insert(session, item);
    } else {
        SessionTreeItem* parent = d.sessionItems.value(view->session());
        item = new SessionTreeItem(view, parent);
    }

    connect(view, SIGNAL(activeChanged()), this, SLOT(updateView()));
    connect(view, SIGNAL(receiverChanged(QString)), this, SLOT(updateView()));
    d.viewItems.insert(view, item);
    updateView(view);
}

void SessionTreeWidget::removeView(MessageView* view)
{
    if (view->viewType() == ViewInfo::Server)
        d.sessionItems.remove(view->session());
    delete d.viewItems.take(view);
}

void SessionTreeWidget::renameView(MessageView* view)
{
    SessionTreeItem* item = d.viewItems.value(view);
    if (item)
        item->setText(0, view->receiver());
}

void SessionTreeWidget::setCurrentView(MessageView* view)
{
    SessionTreeItem* item = d.viewItems.value(view);
    if (item)
        setCurrentItem(item);
}

void SessionTreeWidget::moveToNextItem()
{
    QTreeWidgetItem* item = nextItem(currentItem());
    if (!item)
        item = topLevelItem(0);
    setCurrentItem(item);
}

void SessionTreeWidget::moveToPrevItem()
{
    QTreeWidgetItem* item = previousItem(currentItem());
    if (!item)
        item = lastItem();
    setCurrentItem(item);
}

void SessionTreeWidget::moveToNextActiveItem()
{
    QTreeWidgetItem* item = findNextItem(currentItem(), 0, SessionTreeItem::HighlightRole);
    if (!item)
        item = findNextItem(currentItem(), 1, SessionTreeItem::BadgeRole);
    if (item)
        setCurrentItem(item);
}

void SessionTreeWidget::moveToPrevActiveItem()
{
    QTreeWidgetItem* item = findPrevItem(currentItem(), 0, SessionTreeItem::HighlightRole);
    if (!item)
        item = findPrevItem(currentItem(), 1, SessionTreeItem::BadgeRole);
    if (item)
        setCurrentItem(item);
}

void SessionTreeWidget::moveToMostActiveItem()
{
    SessionTreeItem* mostActive = 0;
    QTreeWidgetItemIterator it(this, QTreeWidgetItemIterator::Unselected);
    while (*it) {
        SessionTreeItem* item = static_cast<SessionTreeItem*>(*it);

        if (item->isHighlighted()) {
            // we found a channel hilight or PM to us
            setCurrentItem(item);
            return;
        }

        // as a backup, store the most active window with any sort of activity
        if (item->badge() && (!mostActive || mostActive->badge() < item->badge()))
            mostActive = item;

        it++;
    }

    if (mostActive)
        setCurrentItem(mostActive);
}

void SessionTreeWidget::search(const QString& search)
{
    if (!search.isEmpty()) {
        QList<QTreeWidgetItem*> items = findItems(search, Qt::MatchContains | Qt::MatchWrap | Qt::MatchRecursive);
        if (!items.isEmpty() && !items.contains(currentItem()))
            setCurrentItem(items.first());
        emit searched(!items.isEmpty());
    }
}

void SessionTreeWidget::searchAgain(const QString& search)
{
    QTreeWidgetItem* item = currentItem();
    if (item && !search.isEmpty()) {
        QTreeWidgetItemIterator it(item, QTreeWidgetItemIterator::Unselected);
        bool wrapped = false;
        while (*it) {
            if ((*it)->text(0).contains(search, Qt::CaseInsensitive)) {
                setCurrentItem(*it);
                return;
            }
            ++it;
            if (!(*it) && !wrapped) {
                it = QTreeWidgetItemIterator(this, QTreeWidgetItemIterator::Unselected);
                wrapped = true;
            }
        }
    }
}

void SessionTreeWidget::blockItemReset()
{
    d.itemResetBlocked = true;
}

void SessionTreeWidget::unblockItemReset()
{
    d.itemResetBlocked = false;
    delayedItemReset();
}

void SessionTreeWidget::expandCurrentSession()
{
    QTreeWidgetItem* item = currentItem();
    if (item && item->parent())
        item = item->parent();
    if (item)
        expandItem(item);
}

void SessionTreeWidget::collapseCurrentSession()
{
    QTreeWidgetItem* item = currentItem();
    if (item && item->parent())
        item = item->parent();
    if (item) {
        collapseItem(item);
        setCurrentItem(item);
    }
}

void SessionTreeWidget::highlight(SessionTreeItem* item)
{
    if (d.highlightedItems.isEmpty())
        SharedTimer::instance()->registerReceiver(this, "highlightTimeout");
    d.highlightedItems.insert(item);
    item->setHighlighted(true);
}

void SessionTreeWidget::unhighlight(SessionTreeItem* item)
{
    if (d.highlightedItems.remove(item) && d.highlightedItems.isEmpty())
        SharedTimer::instance()->unregisterReceiver(this, "highlightTimeout");
    item->setHighlighted(false);
}

void SessionTreeWidget::applySettings(const Settings& settings)
{
    QColor color(settings.colors.value(Settings::Highlight));
    setStatusColor(Highlight, color);

    d.prevShortcut->setKey(QKeySequence(settings.shortcuts.value(Settings::PreviousView)));
    d.nextShortcut->setKey(QKeySequence(settings.shortcuts.value(Settings::NextView)));
    d.prevActiveShortcut->setKey(QKeySequence(settings.shortcuts.value(Settings::PreviousActiveView)));
    d.nextActiveShortcut->setKey(QKeySequence(settings.shortcuts.value(Settings::NextActiveView)));
    d.expandShortcut->setKey(QKeySequence(settings.shortcuts.value(Settings::ExpandView)));
    d.collapseShortcut->setKey(QKeySequence(settings.shortcuts.value(Settings::CollapseView)));
    d.mostActiveShortcut->setKey(QKeySequence(settings.shortcuts.value(Settings::MostActiveView)));
    d.settings = settings;
}

void SessionTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    SessionTreeItem* item = static_cast<SessionTreeItem*>(itemAt(event->pos()));
    if (item) {
        QMenu* menu = menuFactory()->createSessionTreeMenu(item, this);
        menu->exec(event->globalPos());
        menu->deleteLater();
    }
}

void SessionTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());
    if (!item || !item->parent() || item->parent() != d.dropParent)
        event->ignore();
    else
        QTreeWidget::dragMoveEvent(event);
}

bool SessionTreeWidget::event(QEvent* event)
{
    if (event->type() == QEvent::WindowActivate)
        delayedItemReset();
    return QTreeWidget::event(event);
}

QMimeData* SessionTreeWidget::mimeData(const QList<QTreeWidgetItem*> items) const
{
    QTreeWidgetItem* item = items.value(0);
    d.dropParent = item ? item->parent() : 0;
    return QTreeWidget::mimeData(items);
}

void SessionTreeWidget::rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    QTreeWidget::rowsAboutToBeRemoved(parent, start, end);
    SessionTreeItem* item = static_cast<SessionTreeItem*>(itemFromIndex(parent));
    if (item) {
        for (int i = start; i <= end; ++i) {
            SessionTreeItem* child = static_cast<SessionTreeItem*>(item->child(i));
            if (child) {
                item->d.highlightedChildren.remove(child);
                d.resetedItems.remove(child);
                unhighlight(child);
            }
        }
    }
}

void SessionTreeWidget::updateView(MessageView* view)
{
    if (!view)
        view = qobject_cast<MessageView*>(sender());
    SessionTreeItem* item = d.viewItems.value(view);
    if (item) {
        if (!item->parent())
            item->setText(0, item->session()->name().isEmpty() ? item->session()->host() : item->session()->name());
        else
            item->setText(0, view->receiver());
        // re-read MessageView::isActive()
        item->emitDataChanged();
    }
}

void SessionTreeWidget::updateSession(Session* session)
{
    if (!session)
        session = qobject_cast<Session*>(sender());
    SessionTreeItem* item = d.sessionItems.value(session);
    if (item)
        item->setText(0, session->name().isEmpty() ? session->host() : session->name());
}

void SessionTreeWidget::onItemExpanded(QTreeWidgetItem* item)
{
    static_cast<SessionTreeItem*>(item)->emitDataChanged();
}

void SessionTreeWidget::onItemCollapsed(QTreeWidgetItem* item)
{
    static_cast<SessionTreeItem*>(item)->emitDataChanged();
}

void SessionTreeWidget::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    if (!d.itemResetBlocked) {
        if (previous)
            resetItem(static_cast<SessionTreeItem*>(previous));
        delayedItemReset();
    }
    SessionTreeItem* item = static_cast<SessionTreeItem*>(current);
    if (item)
        emit currentViewChanged(item->session(), item->parent() ? item->text(0) : QString());
}

void SessionTreeWidget::delayedItemReset()
{
    SessionTreeItem* item = static_cast<SessionTreeItem*>(currentItem());
    if (item) {
        d.resetedItems.insert(item);
        QTimer::singleShot(500, this, SLOT(delayedItemResetTimeout()));
    }
}

void SessionTreeWidget::delayedItemResetTimeout()
{
    if (!d.resetedItems.isEmpty()) {
        foreach (SessionTreeItem* item, d.resetedItems)
            resetItem(item);
        d.resetedItems.clear();
    }
}

void SessionTreeWidget::highlightTimeout()
{
    bool active = d.highlightColor == d.colors.value(Active);
    d.highlightColor = d.colors.value(active ? Highlight : Active);

    foreach (SessionTreeItem* item, d.highlightedItems) {
        item->emitDataChanged();
        if (SessionTreeItem* p = static_cast<SessionTreeItem*>(item->parent()))
            if (!p->isExpanded())
                p->emitDataChanged();
    }
}

void SessionTreeWidget::resetItem(SessionTreeItem* item)
{
    item->setBadge(0);
    unhighlight(item);
}

QTreeWidgetItem* SessionTreeWidget::lastItem() const
{
    QTreeWidgetItem* item = topLevelItem(topLevelItemCount() - 1);
    if (item->childCount() > 0)
        item = item->child(item->childCount() - 1);
    return item;
}

QTreeWidgetItem* SessionTreeWidget::nextItem(QTreeWidgetItem* from) const
{
    if (!from)
        return 0;
    QTreeWidgetItemIterator it(from);
    while (*++it) {
        if (!(*it)->parent() || (*it)->parent()->isExpanded())
            break;
    }
    return *it;
}

QTreeWidgetItem* SessionTreeWidget::previousItem(QTreeWidgetItem* from) const
{
    if (!from)
        return 0;
    QTreeWidgetItemIterator it(from);
    while (*--it) {
        if (!(*it)->parent() || (*it)->parent()->isExpanded())
            break;
    }
    return *it;
}

QTreeWidgetItem* SessionTreeWidget::findNextItem(QTreeWidgetItem* from, int column, int role) const
{
    if (from) {
        QTreeWidgetItemIterator it(from);
        while (*++it && *it != from) {
            SessionTreeItem* item = static_cast<SessionTreeItem*>(*it);
            if (item->data(column, role).toBool())
                return item;
        }
    }
    return 0;
}

QTreeWidgetItem* SessionTreeWidget::findPrevItem(QTreeWidgetItem* from, int column, int role) const
{
    if (from) {
        QTreeWidgetItemIterator it(from);
        while (*--it && *it != from) {
            SessionTreeItem* item = static_cast<SessionTreeItem*>(*it);
            if (item->data(column, role).toBool())
                return item;
        }
    }
    return 0;
}
