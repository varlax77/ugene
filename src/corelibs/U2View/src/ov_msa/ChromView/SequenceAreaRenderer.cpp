/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
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

#include "SequenceAreaRenderer.h"

#include <U2Algorithm/MsaHighlightingScheme.h>
#include <U2Algorithm/MsaColorScheme.h>

#include <U2Core/U2OpStatusUtils.h>

#include <QPainter>

namespace U2 {

SequenceAreaRenderer::SequenceAreaRenderer(MSAEditorSequenceArea *seqAreaWgt)
    : QObject(seqAreaWgt),
      seqAreaWgt(seqAreaWgt),
      linePen(Qt::gray, 1, Qt::DotLine),
      kLinearTransformTrace(0.0),
      bLinearTransformTrace(0.0) {

    font.setFamily("Courier");
    font.setPointSize(12);
    fontBold = font;
    fontBold.setBold(true);
    QFontMetricsF fm(font);
    charWidth = fm.width('W');
    charHeight = fm.ascent();

    heightPD = 100;// seqAheight(); SANGER_TODO: get from const in seq area
    heightAreaBC = seqAreaWgt->editor->getSequenceRowHeight()/*50*/; // also there should not be such hard-codes consts
    addUpIfQVL = 0;
    areaHeight = heightPD - heightAreaBC;

//    if (chroma.hasQV && p->showQV()) {
//        addUpIfQVL = 0;
//    }
//    else    {
//        addUpIfQVL = heightAreaBC - 2*charHeight;
//        setMinimumHeight(height()-addUpIfQVL);
//        areaHeight = height()-heightAreaBC + addUpIfQVL;
//    }
}

bool SequenceAreaRenderer::drawContent(QPainter &p, const U2Region &region, const QList<qint64> &seqIdx) {
    CHECK(!region.isEmpty(), false);
    CHECK(!seqIdx.isEmpty(), false);

    MsaHighlightingScheme* highlightingScheme = seqAreaWgt->highlightingScheme;
    MSAEditor* editor = seqAreaWgt->editor;

//    p.fillRect(QRect(0, 0, editor->getColumnWidth() * region.length,
//                      editor->getRowHeight() * seqIdx.size()),
//               Qt::white);
    p.setPen(Qt::black);
    p.setFont(editor->getFont());

    MultipleSequenceAlignmentObject* maObj = editor->getMSAObject();
    SAFE_POINT(maObj != NULL, tr("Alignment object is NULL"), false);
    const MultipleSequenceAlignment msa = maObj->getMsa();

    U2OpStatusImpl os;
    const int refSeq = msa->getRowIndexByRowId(editor->getReferenceRowId(), os);
    QString refSeqName = editor->getReferenceRowName();
    MultipleSequenceAlignmentRow row;
    if (U2MsaRow::INVALID_ROW_ID != refSeq) {
        row = msa->getRow(refSeq);
    }

    //Use dots to draw regions, which are similar to reference sequence
    highlightingScheme->setUseDots(seqAreaWgt->useDotsAction->isChecked());
    //Highlighting scheme's settings
    QString schemeName = highlightingScheme->metaObject()->className();
    bool isGapsScheme = schemeName == "U2::MSAHighlightingSchemeGaps";

    U2Region baseYRange = U2Region(0, editor->getSequenceRowHeight());
    int columnWidth = editor->getColumnWidth();

    bool isResizeMode = editor->getResizeMode() == MSAEditor::ResizeMode_FontAndContent;

    // SANGER_TODO: test chrom
    DNAChromatogram chrom;
    chrom.traceLength = 3;
    chrom.seqLength = 3;
    chrom.baseCalls << 5 << 7 << 9;
    chrom.hasQV = false;
    chrom.A << 10 << 20 << 30;
    chrom.C << 20 << 10 << 30;
    chrom.G << 30 << 40 << 30;
    chrom.T << 50 << 30 << 30;
    U2Region testVisRegion(0, 3);

    for (qint64 iSeq = 0; iSeq < seqIdx.size(); iSeq++) {
        qint64 seq = seqIdx[iSeq];
        qint64 regionEnd = region.endPos() - (int)(region.endPos() == editor->getAlignmentLen());

        for (int pos = region.startPos; pos <= regionEnd; pos++) {
            U2Region baseXRange = U2Region(columnWidth * (pos - region.startPos), columnWidth);
            QRect cr(baseXRange.startPos, baseYRange.startPos, baseXRange.length + 1, baseYRange.length);
            char c = msa->charAt(seq, pos);

            bool highlight = false;
            QColor color = seqAreaWgt->colorScheme->getColor(seq, pos, c); //! SANGER_TODO: add NULL checks or do smt with the infrastructure
            if (isGapsScheme || highlightingScheme->getFactory()->isRefFree()) { //schemes which applied without reference
                const char refChar = '\n';
                highlightingScheme->process(refChar, c, color, highlight, pos, seq);
            } else if (seq == refSeq || refSeqName.isEmpty()) {
                highlight = true;
            } else {
                SAFE_POINT(NULL != row, "MSA row is NULL", false);
                const char refChar = row->charAt(pos);
                highlightingScheme->process(refChar, c, color, highlight, pos, seq);
            }

            if (color.isValid() && highlight) {
                p.fillRect(cr, color);
            }
            if (isResizeMode) {
                p.drawText(cr, Qt::AlignCenter, QString(c));
            }
        }
        if (editor->showChromatograms) {
            // SANGER_TODO: draw chromotogram below
            p.save();
            p.translate(0, baseYRange.startPos + editor->getSequenceRowHeight());
            drawChromatogram(p, chrom, testVisRegion);
            p.restore();
        }
        baseYRange.startPos += editor->getRowHeight();
    }

    return true;
}

void SequenceAreaRenderer::drawChromatogram(QPainter &p, DNAChromatogram &chroma, U2Region& visible) {
    // SANGER_TODO: move from the method
    static const QColor colorForIds[4] = { Qt::darkGreen, Qt::blue, Qt::black, Qt::red};
    static const QString baseForIds[4] = { "A", "C", "G", "T" };
    static const qreal dividerTraceOrBaseCallsLines = 2;
    static const qreal dividerBoolShowBaseCallsChars = 1.5;

    // SANGER_TODO: should not be here
    chromaMax = 0;
    for (int i = 0; i < chroma.traceLength; i++)
    {
        if (chromaMax < chroma.A[i]) chromaMax = chroma.A[i];
        if (chromaMax < chroma.C[i]) chromaMax = chroma.C[i];
        if (chromaMax < chroma.G[i]) chromaMax = chroma.G[i];
        if (chromaMax < chroma.T[i]) chromaMax = chroma.T[i];
    }

//    ChromatogramView* chromaView = qobject_cast<ChromatogramView*>(view);

//    const U2Region& visible = view->getVisibleRange();
    assert(!visible.isEmpty());

//    ADVSequenceObjectContext* seqCtx = view->getSequenceContext();
//    U2OpStatusImpl os;
//    QByteArray seq = seqCtx->getSequenceObject()->getWholeSequenceData(os);
//    SAFE_POINT_OP(os, );
    QByteArray seq = "ACG"; // SANGER_TODO: tmp

    // SANGER_TODO:
//    GSLV_UpdateFlags uf = view->getUpdateFlags();
    bool completeRedraw = true; //uf.testFlag(GSLV_UF_NeedCompleteRedraw) || uf.testFlag(GSLV_UF_ViewResized) || uf.testFlag(GSLV_UF_VisibleRangeChanged);

    heightPD = 100; // SANGER_TODO: const for chrom height

    if (completeRedraw) {
//        QPainter p(cachedView);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.fillRect(0, 0, seqAreaWgt->width(), heightPD, Qt::lightGray/*Qt::white*/); // SANGER_TODO: test
        if (seqAreaWgt->width() / charWidth > visible.length / dividerBoolShowBaseCallsChars) {
            //draw basecalls
            p.translate(0, heightAreaBC - addUpIfQVL);

            drawOriginalBaseCalls(chroma,
                                  0, heightAreaBC - charHeight - addUpIfQVL, seqAreaWgt->width(), charHeight,
                                  p, visible, seq);

            if (chroma.hasQV /*&& chromaView->showQV()*/) { // SANGER_TODO: deal with settings in are wgt
                drawQualityValues(chroma,
                                  0, charHeight, seqAreaWgt->width(), heightAreaBC - 2*charHeight, p, visible, seq);
            }
        } else {
            QRectF rect(charWidth, 0, seqAreaWgt->width() - 2*charWidth, 2*charHeight);
            p.drawText(rect, Qt::AlignCenter, QString(tr("Chromatogram view (zoom in to see base calls)")));
            int curCP = seqAreaWgt->width() - charWidth;
            for (int i = 0; i < 4; ++i) {
                curCP -= 2 * charWidth;
                p.setPen(colorForIds[i]);
                p.drawRect(curCP + charWidth / 6, heightAreaBC - charHeight, charWidth / 2, - charHeight / 2);
                p.setPen(Qt::black);
                p.drawText(curCP + charWidth, heightAreaBC - charHeight, baseForIds[i]);
            }
        }
//        if (seqAreaWgt->width() / charWidth > visible.length / dividerTraceOrBaseCallsLines) {
//            drawChromatogramTrace(chroma,
//                                  0, heightAreaBC - addUpIfQVL, seqAreaWgt->width(), heightPD - heightAreaBC + addUpIfQVL,
//                p, visible/*, chromaView->getSettings()*/);
//        } else {
//            drawChromatogramBaseCallsLines(chroma,
//                                           0, heightAreaBC, seqAreaWgt->width(), heightPD - heightAreaBC,
//                p, visible, seq/*, chromaView->getSettings()*/);
//        }
    }
////    QPainter p(pd);
//    p.setFont(font);
//    p.drawPixmap(0, 0, *cachedView);

    // SANGER_TODO: draw selection
//    if (hasSel) {
//        p.setPen(linePen);
//        p.drawRect(selRect);
//        hasSel = false;
//    }

    // SANGER_TODO: edit characters - currentBaseCalls - edited ??
//    if (pd->width() / charWidth > visible.length /dividerBoolShowBaseCallsChars && false/*chromaView->editDNASeq!=NULL*/) {
//        drawOriginalBaseCalls(0, 0, width(), charHeight, p, visible, chromaView->currentBaseCalls, false);
//    }

//    const QVector<U2Region>& sel=seqCtx->getSequenceSelection()->getSelectedRegions();
//    if(!sel.isEmpty()) {
//        //draw current selection
//        //selection base on trace transform coef
//        QPen linePenSelection(Qt::darkGray, 1, Qt::SolidLine);
//        p.setPen(linePenSelection);
//        p.setRenderHint(QPainter::Antialiasing, false);

//        U2Region self=sel.first();
//        int i1=self.startPos,i2=self.endPos()-1;
//        unsigned int startBaseCall = kLinearTransformTrace * chroma.baseCalls[i1];
//        unsigned int endBaseCall = kLinearTransformTrace * chroma.baseCalls[i2];
//        if (i1!=0)  {
//            unsigned int prevBaseCall = kLinearTransformTrace * chroma.baseCalls[i1-1];
//            p.drawLine((startBaseCall + prevBaseCall) / 2 + bLinearTransformTrace, 0,
//                (startBaseCall+ prevBaseCall)/2 + bLinearTransformTrace, pd->height());
//        }else {
//            p.drawLine(startBaseCall + bLinearTransformTrace - charWidth / 2, 0,
//                startBaseCall + bLinearTransformTrace - charWidth / 2, pd->height());
//        }
//        if (i2!=chroma.seqLength-1) {
//            unsigned int nextBaseCall = kLinearTransformTrace * chroma.baseCalls[i2+1];
//            p.drawLine((endBaseCall + nextBaseCall) / 2 + bLinearTransformTrace, 0,
//                (endBaseCall + nextBaseCall) / 2 + bLinearTransformTrace, pd->height());
//        } else {
//            p.drawLine(endBaseCall + bLinearTransformTrace + charWidth / 2, 0,
//                endBaseCall + bLinearTransformTrace + charWidth / 2, pd->height());
//        }
//    }

}

QColor SequenceAreaRenderer::getBaseColor( char base )
{

    switch(base) {
        case 'A':
            return Qt::darkGreen;
        case 'C':
            return Qt::blue;
        case 'G':
            return Qt::black;
        case 'T':
            return Qt::red;
        default:
            return Qt::black;
    }

}

//draw functions

void SequenceAreaRenderer::drawChromatogramTrace(const DNAChromatogram& chroma,
                                                 qreal x, qreal y, qreal w, qreal h, QPainter& p,
                                                 const U2Region& visible/*, const ChromatogramViewSettings& settings*/)
{
    if (chromaMax == 0) {
        //nothing to draw
        return;
    }
    //founding problems

    //areaHeight how to define startValue?
    //colorForIds to private members
    static const QColor colorForIds[4] = {
        Qt::darkGreen, Qt::blue, Qt::black, Qt::red
    };
    p.setRenderHint(QPainter::Antialiasing, true);
//    p.resetTransform();
//    p.translate(x,y + h);

    //drawBoundingRect SANGER_TODO: was commented originally
    p.drawLine(0,0,w,0);
    p.drawLine(0,-h,w,-h);
    p.drawLine(0,0,0,-h);
    p.drawLine(w,0,w,-h);


    int a1 = chroma.baseCalls[visible.startPos];
    int a2 = chroma.baseCalls[visible.endPos()-1];
    qreal leftMargin, rightMargin;
    leftMargin = rightMargin = charWidth;
    qreal k1 = w - leftMargin  - rightMargin;
    int k2 = a2 - a1;
    kLinearTransformTrace = qreal (k1) / k2;
    bLinearTransformTrace = leftMargin - kLinearTransformTrace*a1;
    int mk1 = qMin(static_cast<int>(leftMargin / kLinearTransformTrace), a1);
    int mk2 = qMin(static_cast<int>(rightMargin / kLinearTransformTrace), chroma.traceLength - a2 - 1);
    int polylineSize = a2-a1+mk1+mk2+1;
    QPolygonF polylineA(polylineSize), polylineC(polylineSize),
        polylineG(polylineSize), polylineT(polylineSize);
    int areaHeight = (heightPD - heightAreaBC + addUpIfQVL) * this->areaHeight / 100;
    for (int j = a1-mk1; j <= a2+mk2; ++j) {
        double x = kLinearTransformTrace*j+bLinearTransformTrace;
        qreal yA = -qMin(static_cast<qreal>(chroma.A[j]) * areaHeight / chromaMax, h);
        qreal yC = -qMin(static_cast<qreal>(chroma.C[j]) * areaHeight / chromaMax, h);
        qreal yG = -qMin(static_cast<qreal>(chroma.G[j]) * areaHeight / chromaMax, h);
        qreal yT = -qMin(static_cast<qreal>(chroma.T[j]) * areaHeight / chromaMax, h);
        polylineA[j-a1+mk1] = QPointF(x, yA);
        polylineC[j-a1+mk1] = QPointF(x, yC);
        polylineG[j-a1+mk1] = QPointF(x, yG);
        polylineT[j-a1+mk1] = QPointF(x, yT);
    }
    if (settings.drawTraceA) {
        p.setPen(colorForIds[0]);
        p.drawPolyline(polylineA);
    }
    if (settings.drawTraceC) {
        p.setPen(colorForIds[1]);
        p.drawPolyline(polylineC);
    }
    if (settings.drawTraceG) {
        p.setPen(colorForIds[2]);
        p.drawPolyline(polylineG);
    }
    if (settings.drawTraceT) {
        p.setPen(colorForIds[3]);
        p.drawPolyline(polylineT);
    }
//    p.resetTransform();
//    p.translate(- x, - y - h);
}

void SequenceAreaRenderer::drawOriginalBaseCalls(const DNAChromatogram& chroma, qreal x, qreal y, qreal w, qreal h,
                                                 QPainter& p, const U2Region& visible, const QByteArray& ba, bool is)
{
    QRectF rect;

    p.setPen(Qt::black);
//    p.resetTransform();
//    p.translate( x, y + h);


    int a1 = chroma.baseCalls[visible.startPos];
    int a2 = chroma.baseCalls[visible.endPos()-1];
    qreal leftMargin, rightMargin;
    leftMargin = rightMargin = charWidth;
    qreal k1 = w - leftMargin  - rightMargin;
    int k2 = a2 - a1;
    qreal kLinearTransformBaseCalls = qreal (k1) / k2;
    qreal bLinearTransformBaseCalls = leftMargin - kLinearTransformBaseCalls*a1;

    if (!is)    {
        kLinearTransformBaseCallsOfEdited = kLinearTransformBaseCalls;
        bLinearTransformBaseCallsOfEdited = bLinearTransformBaseCalls;
        xBaseCallsOfEdited = x;
        yBaseCallsOfEdited = y;
        wBaseCallsOfEdited = w;
        hBaseCallsOfEdited = h;
    }
//    ChromatogramView* cview = qobject_cast<ChromatogramView*>(view);
    for (int i=visible.startPos;i<visible.endPos();i++) {
        QColor color = getBaseColor(ba[i]);
        p.setPen(color);

        // SANGER_TODO: dealing with modified characters (do not forget to remove the upper commented line)
        if (false/*cview->indexOfChangedChars.contains(i)*/ && !is) {
            p.setFont(fontBold);
        } else {
            p.setFont(font);
        }
        int xP = kLinearTransformBaseCalls*chroma.baseCalls[i] + bLinearTransformBaseCalls;
        rect.setRect(xP - charWidth/2 + linePen.width(), -h, charWidth, h);
        p.drawText(rect, Qt::AlignCenter, QString(ba[i]));

        if (is) {
            p.setPen(linePen);
            p.setRenderHint(QPainter::Antialiasing, false);
            p.drawLine(xP, 0, xP, heightPD - y);
        }
    }

    if (is) {
        p.setPen(linePen);
        p.setFont(QFont(QString("Courier New"), 8));
        p.drawText(charWidth*1.3, charHeight/2, QString(tr("original sequence")));
    }
//    p.resetTransform();
//    p.translate( - x, - y - h);
}

void SequenceAreaRenderer::drawQualityValues(const DNAChromatogram& chroma,
                                             qreal x, qreal y, qreal w, qreal h,
                                             QPainter& p, const U2Region& visible, const QByteArray& ba)
{
    QRectF rectangle;

//    p.resetTransform();
//    p.translate(x,y+h);

    //draw grid
    p.setPen(linePen);
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < 5; ++i) p.drawLine(0,-h*i/4, w, -h*i/4);

     QLinearGradient gradient(10, 0, 10, -h);
     gradient.setColorAt(0, Qt::green);
     gradient.setColorAt(0.33, Qt::yellow);
     gradient.setColorAt(0.66, Qt::red);
     QBrush brush(gradient);

     p.setBrush(brush);
     p.setPen(Qt::black);
     p.setRenderHint(QPainter::Antialiasing, true);



     int a1 = chroma.baseCalls[visible.startPos];
     int a2 = chroma.baseCalls[visible.endPos()-1];
     qreal leftMargin, rightMargin;
     leftMargin = rightMargin = charWidth;
     qreal k1 = w - leftMargin  - rightMargin;
     int k2 = a2 - a1;
     qreal kLinearTransformQV = qreal (k1) / k2;
     qreal bLinearTransformQV = leftMargin - kLinearTransformQV*a1;

     for (int i=visible.startPos;i<visible.endPos();i++) {
         int xP = kLinearTransformQV*chroma.baseCalls[i] + bLinearTransformQV - charWidth/2 + linePen.width();
         switch (ba[i])  {
             case 'A':
                 rectangle.setCoords(xP, 0, xP+charWidth, -h/100*chroma.prob_A[i]);
                 break;
             case 'C':
                 rectangle.setCoords(xP, 0, xP+charWidth, -h/100*chroma.prob_C[i]);
                 break;
             case 'G':
                 rectangle.setCoords(xP, 0, xP+charWidth, -h/100*chroma.prob_G[i]);
                 break;
             case 'T':
                 rectangle.setCoords(xP, 0, xP+charWidth, -h/100*chroma.prob_T[i]);
                 break;
         }
         if (qAbs( rectangle.height() ) > h/100) {
            p.drawRoundedRect(rectangle, 1.0, 1.0);
         }

     }

//     p.resetTransform();
//     p.translate( - x, - y - h);
}


void SequenceAreaRenderer::drawChromatogramBaseCallsLines(const DNAChromatogram& chroma,
                                                          qreal x, qreal y, qreal w, qreal h, QPainter& p,
                                                                const U2Region& visible, const QByteArray& ba//,
                                                          /*const ChromatogramViewSettings& settings*/)
{
    static const QColor colorForIds[4] = {
        Qt::darkGreen, Qt::blue, Qt::black, Qt::red
    };
    p.setRenderHint(QPainter::Antialiasing, false);
//    p.resetTransform();
//    p.translate(x,y+h);

    /*  //drawBoundingRect
    p.drawLine(0,0,w,0);
    p.drawLine(0,-h,w,-h);
    p.drawLine(0,0,0,-h);
    p.drawLine(w,0,w,-h);*/


    int a1 = chroma.baseCalls[visible.startPos];
    int a2 = chroma.baseCalls[visible.endPos()-1];
    qreal leftMargin, rightMargin;
    leftMargin = rightMargin = linePen.width();
    qreal k1 = w - leftMargin  - rightMargin;
    int k2 = a2 - a1;
    kLinearTransformTrace = qreal (k1) / k2;
    bLinearTransformTrace = leftMargin - kLinearTransformTrace*a1;
    double yRes = 0;
    int areaHeight = (heightPD - heightAreaBC + addUpIfQVL) * this->areaHeight / 100;
    for (int j = visible.startPos; j < visible.startPos+visible.length; j++) {
        int temp = chroma.baseCalls[j];
        if (temp >= chroma.traceLength) {
            // damaged data - FIXME improve?
            break;
        }
        double x = kLinearTransformTrace*temp+bLinearTransformTrace;
        bool drawBase = true;
        switch (ba[j])  {
            case 'A':
                yRes = -qMin(static_cast<qreal>(chroma.A[temp])*areaHeight/chromaMax, h);
                p.setPen(colorForIds[0]);
                drawBase = settings.drawTraceA;
                break;
            case 'C':
                yRes = -qMin(static_cast<qreal>(chroma.C[temp]) * areaHeight / chromaMax, h);
                p.setPen(colorForIds[1]);
                drawBase = settings.drawTraceC;
                break;
            case 'G':
                yRes = -qMin(static_cast<qreal>(chroma.G[temp]) * areaHeight / chromaMax, h);
                p.setPen(colorForIds[2]);
                drawBase = settings.drawTraceG;
                break;
            case 'T':
                yRes = -qMin(static_cast<qreal>(chroma.T[temp]) * areaHeight / chromaMax, h);
                p.setPen(colorForIds[3]);
                drawBase = settings.drawTraceT;
                break;
            case 'N':
                continue;
        };
        if (drawBase) {
            p.drawLine(x, 0, x, yRes);
        }
    }
//    p.resetTransform();
//    p.translate( - x, - y - h);
}

} // namespace
