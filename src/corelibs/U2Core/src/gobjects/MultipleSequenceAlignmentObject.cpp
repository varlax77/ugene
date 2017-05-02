/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2017 UniPro <ugene@unipro.ru>
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

#include <U2Core/DocumentModel.h>
#include <U2Core/GHints.h>
#include <U2Core/GObjectTypes.h>
#include <U2Core/MsaDbiUtils.h>
#include <U2Core/MultipleSequenceAlignmentExporter.h>
#include <U2Core/MultipleSequenceAlignmentImporter.h>
#include <U2Core/U2AlphabetUtils.h>
#include <U2Core/U2DbiUtils.h>
#include <U2Core/U2ObjectDbi.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>

#include "MultipleSequenceAlignmentObject.h"

namespace U2 {

MultipleSequenceAlignmentObject::MultipleSequenceAlignmentObject(const QString &name,
                                                                 const U2EntityRef &msaRef,
                                                                 const QVariantMap &hintsMap,
                                                                 const MultipleSequenceAlignment &alnData)
    : MultipleAlignmentObject(GObjectTypes::MULTIPLE_SEQUENCE_ALIGNMENT, name, msaRef, hintsMap, alnData)
{

}

const MultipleSequenceAlignment MultipleSequenceAlignmentObject::getMsa() const {
    return getMultipleAlignment().dynamicCast<MultipleSequenceAlignment>();
}

const MultipleSequenceAlignment MultipleSequenceAlignmentObject::getMsaCopy() const {
    return getMsa()->getExplicitCopy();
}

GObject * MultipleSequenceAlignmentObject::clone(const U2DbiRef &dstDbiRef, U2OpStatus &os, const QVariantMap &hints) const {
    DbiOperationsBlock opBlock(dstDbiRef, os);
    Q_UNUSED(opBlock);
    CHECK_OP(os, NULL);

    QScopedPointer<GHintsDefaultImpl> gHints(new GHintsDefaultImpl(getGHintsMap()));
    gHints->setAll(hints);
    const QString dstFolder = gHints->get(DocumentFormat::DBI_FOLDER_HINT, U2ObjectDbi::ROOT_FOLDER).toString();

    MultipleSequenceAlignment msa = getMsa()->getExplicitCopy();
    MultipleSequenceAlignmentObject *clonedObj = MultipleSequenceAlignmentImporter::createAlignment(dstDbiRef, dstFolder, msa, os);
    CHECK_OP(os, NULL);

    clonedObj->setGHints(gHints.take());
    clonedObj->setIndexInfo(getIndexInfo());
    return clonedObj;
}

char MultipleSequenceAlignmentObject::charAt(int seqNum, qint64 position) const {
    return getMultipleAlignment()->charAt(seqNum, position);
}

const MultipleSequenceAlignmentRow MultipleSequenceAlignmentObject::getMsaRow(int row) const {
    return getRow(row).dynamicCast<MultipleSequenceAlignmentRow>();
}

void MultipleSequenceAlignmentObject::updateGapModel(U2OpStatus &os, const U2MsaMapGapModel &rowsGapModel) {
    SAFE_POINT(!isStateLocked(), "Alignment state is locked", );

    const MultipleSequenceAlignment msa = getMultipleAlignment();

    const QList<qint64> rowIds = msa->getRowsIds();
    QList<qint64> modifiedRowIds;
    foreach (qint64 rowId, rowsGapModel.keys()) {
        if (!rowIds.contains(rowId)) {
            os.setError("Can't update gaps of a multiple alignment");
            return;
        }

        MaDbiUtils::updateRowGapModel(entityRef, rowId, rowsGapModel.value(rowId), os);
        CHECK_OP(os, );
        modifiedRowIds.append(rowId);
    }

    MaModificationInfo mi;
    mi.rowListChanged = false;
    updateCachedMultipleAlignment(mi);
}

void MultipleSequenceAlignmentObject::updateGapModel(const QList<MultipleSequenceAlignmentRow> &sourceRows) {
    const QList<MultipleSequenceAlignmentRow> oldRows = getMsa()->getMsaRows();

    SAFE_POINT(oldRows.count() == sourceRows.count(), "Different rows count", );

    U2MsaMapGapModel newGapModel;
    QList<MultipleSequenceAlignmentRow>::ConstIterator oldRowsIterator = oldRows.begin();
    QList<MultipleSequenceAlignmentRow>::ConstIterator sourceRowsIterator = sourceRows.begin();
    for (; oldRowsIterator != oldRows.end(); oldRowsIterator++, sourceRowsIterator++) {
        newGapModel[(*oldRowsIterator)->getRowId()] = (*sourceRowsIterator)->getGapModel();
    }

    U2OpStatus2Log os;
    updateGapModel(os, newGapModel);
}

U2MsaMapGapModel MultipleSequenceAlignmentObject::getGapModel() const {
    U2MsaMapGapModel rowsGapModel;
    foreach (const MultipleSequenceAlignmentRow &curRow, getMsa()->getMsaRows()) {
        rowsGapModel[curRow->getRowId()] = curRow->getGapModel();
    }
    return rowsGapModel;
}

void MultipleSequenceAlignmentObject::insertGap(const U2Region &rows, int pos, int nGaps) {
    MultipleAlignmentObject::insertGap(rows, pos, nGaps, false);
}

void MultipleSequenceAlignmentObject::crop(const U2Region &window, const QSet<QString> &rowNames) {
    SAFE_POINT(!isStateLocked(), "Alignment state is locked", );
    const MultipleSequenceAlignment &ma = getMultipleAlignment();

    QList<qint64> rowIds;
    for (int i = 0; i < ma->getNumRows(); ++i) {
        QString rowName = ma->getRow(i)->getName();
        if (rowNames.contains(rowName)) {
            qint64 rowId = ma->getRow(i)->getRowId();
            rowIds.append(rowId);
        }
    }

    U2OpStatus2Log os;
    MsaDbiUtils::crop(entityRef, rowIds, window.startPos, window.length, os);
    SAFE_POINT_OP(os, );

    updateCachedMultipleAlignment();
}

void MultipleSequenceAlignmentObject::dbiInsertGap(const U2EntityRef& msaRef, const QList<qint64>& rowIds, qint64 pos, qint64 count, bool collapseTrailingGaps, U2OpStatus& os) {
    MsaDbiUtils::insertGaps(msaRef, rowIds, pos, count, os, collapseTrailingGaps);

}

void MultipleSequenceAlignmentObject::deleteColumnWithGaps(U2OpStatus &os, int requiredGapCount) {
    QList<qint64> colsForDelete = getColumnsWithGaps(requiredGapCount);
    if (getLength() == colsForDelete.count()) {
        return;
    }

    QList<U2Region> horizontalRegionsToDelete;
    foreach (qint64 columnNumber, colsForDelete) {
        bool columnMergedWithPrevious = false;
        if (!horizontalRegionsToDelete.isEmpty()) {
            U2Region &lastRegion = horizontalRegionsToDelete.last();
            if (lastRegion.startPos == columnNumber + 1) {
                --lastRegion.startPos;
                ++lastRegion.length;
                columnMergedWithPrevious = true;
            } else if (lastRegion.endPos() == columnNumber) {
                ++lastRegion.length;
                columnMergedWithPrevious = true;
            }
        }

        if (!columnMergedWithPrevious) {
            horizontalRegionsToDelete.append(U2Region(columnNumber, 1));
        }
    }

    QList<U2Region>::const_iterator columns = horizontalRegionsToDelete.constBegin();
    const QList<U2Region>::const_iterator end = horizontalRegionsToDelete.constEnd();

    for (int counter = 0; columns != end; ++columns, counter++) {
        removeRegion((*columns).startPos, 0, (*columns).length, getNumRows(), true, (end - 1 == columns));
        os.setProgress(100 * counter / horizontalRegionsToDelete.size());
    }
    updateCachedMultipleAlignment();
}

void MultipleSequenceAlignmentObject::deleteColumnWithGaps(int requiredGapCount) {
    U2OpStatusImpl os;
    deleteColumnWithGaps(os, requiredGapCount);
    SAFE_POINT_OP(os, );
}

QList<qint64> MultipleSequenceAlignmentObject::getColumnsWithGaps(int requiredGapCount) const {
    const MultipleSequenceAlignment &ma = getMultipleAlignment();
    const int length = ma->getLength();
    if (GAP_COLUMN_ONLY == requiredGapCount) {
        requiredGapCount = ma->getNumRows();
    }
    QList<qint64> colsForDelete;
    for (int i = 0; i < length; i++) { //columns
        int gapCount = 0;
        for (int j = 0; j < ma->getNumRows(); j++) { //sequences
            if (ma->isGap(j, i)) {
                gapCount++;
            }
        }

        if (gapCount >= requiredGapCount) {
            colsForDelete.prepend(i); //invert order
        }
    }
    return colsForDelete;
}

void MultipleSequenceAlignmentObject::updateRow(U2OpStatus &os, int rowIdx, const QString &name, const QByteArray &seqBytes, const U2MsaRowGapModel &gapModel) {
    SAFE_POINT(!isStateLocked(), "Alignment state is locked", );

    const MultipleSequenceAlignment msa = getMultipleAlignment();
    SAFE_POINT(rowIdx >= 0 && rowIdx < msa->getNumRows(), "Invalid row index", );
    qint64 rowId = msa->getRow(rowIdx)->getRowId();

    MsaDbiUtils::updateRowContent(entityRef, rowId, seqBytes, gapModel, os);
    CHECK_OP(os, );

    MaDbiUtils::renameRow(entityRef, rowId, name, os);
    CHECK_OP(os, );
}

void MultipleSequenceAlignmentObject::replaceCharacter(int startPos, int rowIndex, char newChar) {
    SAFE_POINT(!isStateLocked(), "Alignment state is locked", );
    const MultipleSequenceAlignment msa = getMultipleAlignment();
    SAFE_POINT(rowIndex >= 0 && startPos + 1 <= msa->getLength(), "Invalid parameters", );
    qint64 modifiedRowId = msa->getRow(rowIndex)->getRowId();

    U2OpStatus2Log os;
    if (newChar != U2Msa::GAP_CHAR) {
        MsaDbiUtils::replaceCharacterInRow(entityRef, modifiedRowId, startPos, newChar, os);
    } else {
        MsaDbiUtils::removeRegion(entityRef, QList<qint64>() << modifiedRowId, startPos, 1, os);
        MsaDbiUtils::insertGaps(entityRef, QList<qint64>() << modifiedRowId, startPos, 1, os, false);
    }
    SAFE_POINT_OP(os, );

    MaModificationInfo mi;
    mi.rowContentChanged = true;
    mi.rowListChanged = false;
    mi.alignmentLengthChanged = false;
    mi.modifiedRowIds << modifiedRowId;

    if (newChar != ' ' && !msa->getAlphabet()->contains(newChar)) {
        const DNAAlphabet *alp = U2AlphabetUtils::findBestAlphabet(QByteArray(1, newChar));
        const DNAAlphabet *newAlphabet = U2AlphabetUtils::deriveCommonAlphabet(alp, msa->getAlphabet());
        SAFE_POINT(NULL != newAlphabet, "Common alphabet is NULL", );

        if (newAlphabet->getId() != msa->getAlphabet()->getId()) {
            MaDbiUtils::updateMaAlphabet(entityRef, newAlphabet->getId(), os);
            mi.alphabetChanged = true;
            SAFE_POINT_OP(os, );
        }
    }

    updateCachedMultipleAlignment(mi);
}

void MultipleSequenceAlignmentObject::loadAlignment(U2OpStatus &os) {
    MultipleSequenceAlignmentExporter msaExporter;
    cachedMa = msaExporter.getAlignment(entityRef.dbiRef, entityRef.entityId, os);
}

void MultipleSequenceAlignmentObject::updateCachedRows(U2OpStatus &os, const QList<qint64> &rowIds) {
    MultipleSequenceAlignment cachedMsa = cachedMa.dynamicCast<MultipleSequenceAlignment>();

    MultipleSequenceAlignmentExporter msaExporter;
    QList<MsaRowReplacementData> rowsAndSeqs = msaExporter.getAlignmentRows(entityRef.dbiRef, entityRef.entityId, rowIds, os);
    SAFE_POINT_OP(os, );
    foreach (const MsaRowReplacementData &data, rowsAndSeqs) {
        const int rowIndex = cachedMsa->getRowIndexByRowId(data.row.rowId, os);
        SAFE_POINT_OP(os, );
        cachedMsa->setRowContent(rowIndex, data.sequence.seq);
        cachedMsa->setRowGapModel(rowIndex, data.row.gaps);
        cachedMsa->renameRow(rowIndex, data.sequence.getName());
    }
}

void MultipleSequenceAlignmentObject::updateDatabase(U2OpStatus &os, const MultipleAlignment &ma) {
    const MultipleSequenceAlignment msa = ma.dynamicCast<MultipleSequenceAlignment>();
    MsaDbiUtils::updateMsa(entityRef, msa, os);
}

void MultipleSequenceAlignmentObject::removeRowPrivate(U2OpStatus &os, const U2EntityRef &msaRef, qint64 rowId) {
    MsaDbiUtils::removeRow(msaRef, rowId, os);
}

void MultipleSequenceAlignmentObject::removeRegionPrivate(U2OpStatus &os, const U2EntityRef &maRef, const QList<qint64> &rows,
                                                          int startPos, int nBases) {
    MsaDbiUtils::removeRegion(maRef, rows, startPos, nBases, os);
}

}   // namespace U2
