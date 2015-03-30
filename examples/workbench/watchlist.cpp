//
// watchlist.cpp
// This file is part of Ren Garden
// Copyright (C) 2015 MetÆducation
//
// Ren Garden is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Ren Garden is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ren Garden.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://ren-garden.metaeducation.com for more information on this project
//

#include <cstdlib>  // std::abs for integers
#include <iterator>
#include <stdexcept>

#include <QtWidgets>
#include <QTableWidget>

#include "watchlist.h"
#include "mainwindow.h"

#include "rencpp/ren.hpp"

using namespace ren;



//
// WATCH WIDGET ITEM REPRESENTING A SINGLE CELL IN THE WATCH TABLE
//

//
// Currently no support for editing the *values*, only the labels for the
// names.  Technically speaking, value editing isn't difficult, and should
// be included at some point.
//

void WatchList::onItemChanged(QTableWidgetItem * item) {
    Watcher & w = *watchers[item->row()];

    switch (item->column()) {
        case 0: {
            QString contents = item->data(Qt::DisplayRole).toString();

            if (contents == w.getWatchString()) {
                // If the text is the same as what it was, then odds are they
                // selected in the cell and clicked away...vs having the
                // expression (x + y) and wanting to label it (x + y), for
                // instance.
                return;
            }

            // If they type text into the first column and it doesn't match the
            // text that was there, then consider it to be a label.  If they
            // delete everything, consider it to be "unlabeling"

            if (contents.isEmpty())
                w.label = std::experimental::nullopt;
            else
                w.label = Tag {contents};

            updateWatcher(item->row() + 1);
            break;
        }

        case 1:
            reportStatus(
                "Memory editing of variables not supported (yet...)"
            );
            break;

        default:
            throw std::runtime_error("Illegal column selected");
    }
}



//
// WATCHER CLASS REPRESENTING A SINGLE WATCHED VALUE OR EXPRESSION
//


WatchList::Watcher::Watcher (
    Value const & watch,
    bool recalculates,
    std::experimental::optional<Tag> const & label
) :
    watch (watch),
    recalculates (recalculates),
    label (label),
    frozen (false)
{
    evaluate(true);
}


void WatchList::Watcher::evaluate(bool firstTime) {
    try {
        if (firstTime or (recalculates and (not frozen)))
            value = watch.apply();
        error = none;
    }
    catch (evaluation_error const & e) {
        value = unset;
        error = e.error();
    }
    catch (std::exception const & e) {
        assert(false);
        error = none;
    }
    catch (...) {
        assert(false);
    }
}


QString WatchList::Watcher::getWatchString() const {
    if (label) {
        Tag tag = static_cast<Tag>(*label);
        return tag.spellingOf<QString>();
    }

    // Should there be a way to automatically debug based on the
    // address values of the cell?  That could be helpful.  Could
    // also just have a mode that shows that anyway...tooltip?

    return to_QString(watch);
}


QString WatchList::Watcher::getValueString() const {
    if (error)
        return to_QString(error);
    return to_QString(runtime("mold/all quote", value));
}



//
// WATCHLIST CONSTRUCTOR
//

WatchList::WatchList(QWidget * parent) :
    QTableWidget (0, 2, parent)
{
    setHorizontalHeaderLabels(QStringList() << "name" << "value");

    // We want the value column of TableWidget to get wider if the splitter
    // or dock widget give it more space

    horizontalHeader()->setStretchLastSection(true);

    // Lots of interesting options for right click menus on watch items,
    // although we are exploring what can be done with the "watch dialect"
    // to keep your hands off the mouse and still be productive

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        this, &WatchList::customContextMenuRequested,
        this, &WatchList::customMenuRequested,
        Qt::DirectConnection
    );


    // We also have to hook up the watchCalled and handleWatch signals and
    // slots.  This could be asynchronous, however we actually are
    // looking at some of the values and running the evaluator.  So we
    // have to hold up the running evaluator when watch is called

    connect(
        this, &WatchList::pushWatcherRequested,
        this, &WatchList::pushWatcher,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &WatchList::removeWatcherRequested,
        this, &WatchList::removeWatcher,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &WatchList::freezeItemRequested,
        this, &WatchList::setFreezeState,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &WatchList::itemChanged,
        this, &WatchList::onItemChanged,
        Qt::AutoConnection
    );
}


void WatchList::pushWatcher(Watcher * watcherUnique) {
    // Note that because we are passing a unique pointer, more than one
    // client can not listen to the pushWatcherRequest signal.  Should this
    // be a shared pointer?

    watchers.push_back(std::unique_ptr<Watcher>(watcherUnique));

    int count = rowCount();
    insertRow(count);

    updateWatcher(count + 1);

    emit showDockRequested(this);
}


void WatchList::removeWatcher(int index) {
    watchers.erase(std::begin(watchers) + (index - 1));
    removeRow(index - 1);
}


void WatchList::duplicateWatcher(int index) {
    watchers.insert(
        std::begin(watchers) + (index - 1),
        std::unique_ptr<Watcher> {new Watcher {*watchers[index - 1]}}
    );

    blockSignals(true);
    insertRow(index - 1);
    blockSignals(false);

    updateWatcher(index);
}


void WatchList::setFreezeState(int index, bool frozen) {
    watchers[index - 1]->frozen = frozen;
    updateWatcher(index);
}


void WatchList::setRecalculatesState(int index, bool recalculates) {
    watchers[index - 1]->recalculates = recalculates;
    updateWatcher(index);
}


void WatchList::mousePressEvent(QMouseEvent * event) {
    setSelectionMode(QAbstractItemView::SingleSelection);
    QTableWidget::mousePressEvent(event);
}


void WatchList::updateWatcher(int index) {
    Watcher & w = *watchers[index - 1];

    w.evaluate(); // will not evaluate if frozen

    // We only insert a row at the table call sites (duplicate, add); this
    // won't have a table widget item by default.  Hence the if null check.
    // with an allocate.  REVIEW.

    blockSignals(true);

    QTableWidgetItem * nameItem = item(index - 1, 0);
    if (not nameItem) {
        nameItem = new QTableWidgetItem;
        setItem(index - 1, 0, nameItem);
    }
    QTableWidgetItem * valueItem = item(index - 1, 1);
    if (not valueItem) {
        valueItem = new QTableWidgetItem;
        setItem(index - 1, 1, valueItem);
    }

    if (not w.recalculates)
        valueItem->setForeground(Qt::darkGreen);
    else if (w.error)
        valueItem->setForeground(Qt::darkRed);
    else
        valueItem->setForeground(Qt::black);

    if (w.frozen) {
        nameItem->setBackground(Qt::gray);
        valueItem->setBackground(Qt::gray);
    } else {
        nameItem->setBackground(Qt::white);
        valueItem->setBackground(Qt::white);
    }

    if (w.label)
        nameItem->setForeground(Qt::darkMagenta);
    else
        nameItem->setForeground(Qt::black);

    // We give visual feedback by selecting the watches that have changed
    // since the last update.
    //
    // REVIEW: Need to check for value/error change, not just the text
    // changing

    QString newText = w.getValueString();

    nameItem->setSelected(false);
    nameItem->setText(w.getWatchString());
    if (valueItem->text() != newText) {
        valueItem->setText(newText);
        valueItem->setSelected(true);
    }
    else
        valueItem->setSelected(false);

    blockSignals(false);
}


void WatchList::updateAllWatchers() {
    if (not isVisible())
        return;

    // We only temporarily do this selection mode to do our highlighting
    // it's turned off before you can ever click the mouse...

    setSelectionMode(QAbstractItemView::MultiSelection);

    for (int row = 0; row < rowCount(); row++) {
        updateWatcher(row + 1);
    }
}


Value WatchList::watchDialect(
    Value const & arg,
    bool recalculates,
    std::experimental::optional<Tag> const & label
) {
    if (arg.isInteger()) {
        int signedIndex = static_cast<Integer>(arg);
        if (signedIndex == 0)
            throw Error {"Integer arg must be nonzero"};

        bool removal = signedIndex < 0;
        size_t index = std::abs(signedIndex);

        // Positive integer is a request for the data held by the
        // watch, which we need to do here and return synchronously.
        // Negative integers affect the GUI and run on GUI thread.

        if (index > this->watchers.size())
            throw Error {"No such watchlist item index"};

        Value watchValue = watchers[index - 1]->value;
        Value watchError = watchers[index - 1]->error;
        if (removal) {
            emit removeWatcherRequested(index);
            return watchValue;
        }

        if (watchError.isError())
            throw watchError;
        else
            assert(watchError.isNone());

        return watchValue;
    }

    // Let's use the evaluator for this trick.  :-)  Have it give us back
    // -1, 0, or 1... -1 meaning "word is not a logic synonym"

    int logicIndex = -1;

#ifdef GARDEN_CPP_WAY
    if (arg.isWord()) {
        static std::vector<QString> logicWords[2] =
            {{"off", "no", "false"}, {"on", "yes", "true"}};

        auto word = static_cast<Word>(arg);
        auto spelling = word.spellingOf<QString>();
        for (int index = 0; index < 2; index++) {
            if (
                logicWords[index].end()
                != std::find(
                    logicWords[index].begin(),
                    logicWords[index].end(),
                    spelling
                )
            ) {
                logicIndex = index;
                break;
            }
        }
    }

    if (arg.isLogic()) {
        logicIndex = static_cast<bool>(arg);
    }
#else
    logicIndex = static_cast<Integer>(
        runtime(
            "case", Block {
            "    find [off no false] quote", arg, "[0]"
            "    find [on yes true] quote", arg, "[1]"
            "    true [-1]"
            }
        )
    );
#endif

    if (logicIndex != -1) {
        if (logicIndex)
            emit showDockRequested(this);
        else
            emit hideDockRequested(this);
        return unset;
    }

    // With those words out of the way, we should be able to take
    // for granted the creation of a Watcher for words.  Then we
    // can throw an error up to the console on a /cell request.
    // Because there's no use adding it to the watch list if it will
    // never be evaluated again (a regular watch may become good...)

    if (arg.isWord() or arg.isPath() or arg.isParen()) {

        Watcher * watcherUnique = new Watcher {arg, recalculates, label};

        // we append to end instead of inserting at the top because
        // it keeps the numbering more consistent.  But some people might
        // desire it added at the top and consider it better that way?

        emit pushWatcherRequested(watcherUnique);

        Watcher & w = *watcherUnique; // signal handler took ownership

        // it might not seem we need to technically invoke the error here,
        // but if there was an error and people are scripting then they
        // need to handle that with try blocks...because we don't want to
        // quietly give back an unset if the operation failed.

        if (w.error)
            throw w.error;

        return w.value.apply();
    }

    if (arg.isTag()) {
        for (auto & watcherPtr : watchers) {
            Watcher & w = *watcherPtr;
            if (
                w.getWatchString()
                == static_cast<Tag>(arg).spellingOf<QString>()
            ) {
                return w.value;
            }
        }
        throw Error {"unknown tag name in watch list"};
    }

    throw Error {"unexpected type passed to watch dialect"};
}


//
// POPUP CONTEXT MENU
//

//
// Because it's not particularly computationally intensive to create a
// popup menu, we make a new one on each request and use lambda functions
// instead of worrying about setting up a bunch of slots to deal with
// QAction triggers.
//

void WatchList::customMenuRequested(QPoint pos){

    QMenu * menu = new QMenu {this};

    // Review: reconcile mix of indexing
    int index = currentRow() + 1;
    Watcher & w = *watchers[currentRow()];


    //
    // Create the actions for the popup menu
    //

    QAction * frozenAction = new QAction("Frozen", this);
    frozenAction->setCheckable(true);
    frozenAction->setChecked(w.frozen);
    frozenAction->setStatusTip(
        QString("Freeze or unfreeze this watcher, or [watch [freeze +/-")
        + QString::number(index) + "]"
    );

    QAction * recalculatesAction = new QAction("Recalculates", this);
    recalculatesAction->setCheckable(true);
    recalculatesAction->setChecked(w.recalculates);
    recalculatesAction->setStatusTip(
        QString("Watcher calculates or just watches value cell")
    );


    QAction * duplicateAction = new QAction("Duplicate", this);
    duplicateAction->setStatusTip(
        QString("Make a copy of this watcher, or [copy ")
        + QString::number(index) + "]"
    );

    QAction * unwatchAction = new QAction("Unwatch", this);
    unwatchAction->setStatusTip(
        QString("Stop watching this expression, or [watch -")
        + QString::number(index) + "]"
    );


    //
    // Order the items in the menu.  Ideally this would use some kind of
    // heuristic about your common operations to order it (or be configurable)
    // but we try to guess so that the most useful items are at the top,
    // the most dangerous at the bottom, etc.
    //

    menu->addAction(frozenAction);
    menu->addAction(recalculatesAction);
    menu->addAction(duplicateAction);
    menu->addAction(unwatchAction);


    //
    // Connect menu item actions to code that implements them.
    //
    // Note the threading issues tied up here.  We are on the GUI
    // thread, we may be in mid-evaluation of code in the console,
    // etc.  Because we're on the GUI we shouldn't emit the blocking
    // queued signal for requesting a watch.  Review all this.
    //

    connect(
        frozenAction, &QAction::triggered,
        [this, index](bool frozen) {
            setFreezeState(index, frozen);
        }
    );

    connect(
        recalculatesAction, &QAction::triggered,
        [this, index](bool recalculates) {
            setRecalculatesState(index, recalculates);
        }
    );

    connect(
        duplicateAction, &QAction::triggered,
        [this, index](bool) {
            duplicateWatcher(index);
        }
    );


    connect(
        unwatchAction, &QAction::triggered,
        [this, index](bool) {
            removeWatcher(index);
        }
    );

    //
    // Pop up the menu.  Note this call returns while the menu is up, so we
    // can't just delete it...we instead register a hook to clean it up when
    // it's hidden.
    //

    menu->popup(viewport()->mapToGlobal(pos));

    connect(
        menu, &QMenu::aboutToHide,
        menu, &QObject::deleteLater,
        Qt::DirectConnection // it's "deleteLater", already queued...
    );

}

