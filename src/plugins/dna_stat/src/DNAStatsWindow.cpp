/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2017 UniPro <ugene@unipro.ru>
 * http://ugene.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <U2Core/AppContext.h>
#include <U2Core/DNASequenceObject.h>
#include <U2View/ADVSequenceObjectContext.h>

#include "DNAStatsWindow.h"
#include "DNAStatProfileTask.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QContextMenuEvent>

namespace U2 {

DNAStatsWindow::DNAStatsWindow(ADVSequenceObjectContext* context)
: MWMDIWindow(tr("Statistics for %1").arg(context->getSequenceObject()->getGObjectName())),
              ctx(context), updateTask(NULL)
{
    QVBoxLayout* l = new QVBoxLayout(this);
    l->setMargin(0);
    setLayout(l);

    webView = new DNAStatsWebView(this);
    webView->addAction(new QAction("New action!", this));
    webView->setObjectName("DNAStatWebView" + context->getSequenceObject()->getGObjectName());
    l->addWidget(webView);
    connect(AppContext::getTaskScheduler(), SIGNAL(si_stateChanged(Task*)), SLOT(sl_onTaskStateChanged(Task*)));

    update();
}

void DNAStatsWindow::update()
{
    if (updateTask == NULL) {
        updateTask = new DNAStatProfileTask(ctx);
        AppContext::getTaskScheduler()->registerTopLevelTask(updateTask);
    }

}

void DNAStatsWindow::sl_onTaskStateChanged( Task* task )
{
    if (task == updateTask && task->getState() == Task::State_Finished) {
        webView->setHtml(updateTask->getResult());
        updateTask = NULL;
    }
}


void DNAStatsWebView::contextMenuEvent( QContextMenuEvent* event )
{
    QMenu menu;
    menu.addAction(new QAction("Settings", this));

    menu.exec(event->globalPos());
}

} //namespace
