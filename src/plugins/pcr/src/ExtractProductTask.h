/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2014 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
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

#ifndef _U2_EXTRACT_PRODUCT_TASK_H_
#define _U2_EXTRACT_PRODUCT_TASK_H_

#include <U2Core/DNASequence.h>

#include "InSilicoPcrTask.h"

namespace U2 {

class ExtractProductTask : public Task {
public:
    ExtractProductTask(const InSilicoPcrProduct &product, const U2EntityRef &sequenceRef, const QString &outputFile = "");
    ~ExtractProductTask();

    // Task
    void run();

    /* Moves the document to the main thread */
    Document * takeResult();

private:
    DNASequence getProductSequence();
    AnnotationData getPrimerAnnotation(const QByteArray &primer, int matchLengh, U2Strand::Direction strand, int sequenceLength) const;

private:
    InSilicoPcrProduct product;
    U2EntityRef sequenceRef;
    QString outputFile;

    Document *result;
};

class ExtractProductWrapperTask : public Task {
    Q_OBJECT
public:
    ExtractProductWrapperTask(const InSilicoPcrProduct &product, const U2EntityRef &sequenceRef, const QString &sequenceName, qint64 sequenceLength);

    // Task
    void prepare();
    QList<Task*> onSubTaskFinished(Task *subTask);
    ReportResult report();

private:
    void prepareUrl(const InSilicoPcrProduct &product, const QString &sequenceName, qint64 sequenceLength);

private:
    ExtractProductTask *extractTask;
    QString outputFile;
};

} // U2

#endif // _U2_EXTRACT_PRODUCT_TASK_H_