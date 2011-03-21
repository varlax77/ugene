/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2011 UniPro <ugene@unipro.ru>
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

#include "DocWorkers.h"
#include "CoreLib.h"
#include "GenericReadWorker.h"

#include <U2Lang/WorkflowEnv.h>
#include <U2Core/QVariantUtils.h>
#include <U2Lang/CoreLibConstants.h>
#include <U2Lang/BaseTypes.h>
#include <U2Lang/BaseSlots.h>
#include <U2Lang/BaseAttributes.h>
#include <U2Core/Log.h>
#include <U2Core/DocumentModel.h>
#include <U2Core/AnnotationTableObject.h>
#include <U2Core/GObjectUtils.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/MAlignmentObject.h>
#include <U2Core/TextObject.h>
#include <U2Core/GObjectRelationRoles.h>
#include <U2Core/DNASequence.h>
#include <U2Core/AnnotationData.h>

#include <U2Formats/EMBLGenbankAbstractDocument.h>
#include <U2Core/IOAdapter.h>
#include <U2Core/FailTask.h>
#include <U2Core/LoadDocumentTask.h>
#include <U2Core/AppContext.h>
#include <U2Lang/WorkflowUtils.h>

namespace U2 {
namespace LocalWorkflow {

static int ct = 0;

/*************************************
 * TextReader
 *************************************/
void TextReader::init() {
    urls = WorkflowUtils::expandToUrls(actor->getParameter(BaseAttributes::URL_IN_ATTRIBUTE().getId())->getAttributeValue<QString>());

    assert(ports.size() == 1);
    ch = ports.values().first();
}

bool TextReader::isDone() {
    return done;
}

bool TextReader::isReady() {
    return !isDone();
}

Task *TextReader::tick() {
    if(io && io->isOpen()) {
        QByteArray buf;
        buf.resize(1024);
        buf.fill(0);
        int read = io->readLine(buf.data(), 1024);
        buf.resize(read);
        QVariantMap m; 
        m[BaseSlots::TEXT_SLOT().getId()] = QString(buf);
        m[BaseSlots::URL_SLOT().getId()] = url;
        ch->put( Message(mtype, m));
        if(io->isEof()) {
            io->close();
        }
    } else {
        url = urls.takeFirst();
        IOAdapterFactory *iof = AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(BaseIOAdapters::url2io(url));
        io = iof->createIOAdapter();
        if(!io->open(url,IOAdapterMode_Read)) {
            return new FailTask(tr("Can't load file %1").arg(url));
        }
        if(actor->getParameter(BaseAttributes::READ_BY_LINES_ATTRIBUTE().getId())->getAttributeValue<bool>() == false) {
            QByteArray buf;
            int read = 0;
            int offs = 0;
            static const int READ_BLOCK_SZ = 1024;
            buf.resize(READ_BLOCK_SZ);
            buf.fill(0);
            do {
                read = io->readBlock( buf.data() + offs, READ_BLOCK_SZ );
                if( read != READ_BLOCK_SZ ) {
                    assert(read < READ_BLOCK_SZ);
                    buf.resize(buf.size() - READ_BLOCK_SZ + read);
                    break;
                }
                offs += read;
                buf.resize( offs + READ_BLOCK_SZ );
            } while(read == READ_BLOCK_SZ);
            
            QVariantMap m; 
            m[BaseSlots::TEXT_SLOT().getId()] = QString(buf);
            m[BaseSlots::URL_SLOT().getId()] = url;
            ch->put( Message(mtype, m));
            io->close();
        } else {
            QByteArray buf;
            buf.resize(1024);
            buf.fill(0);
            int read = io->readLine(buf.data(), 1024);
            buf.resize(read);
            QVariantMap m; 
            QString str(buf);
            m[BaseSlots::TEXT_SLOT().getId()] = QString(buf);
            m[BaseSlots::URL_SLOT().getId()] = url;
            ch->put( Message(mtype, m));
            if(io->isEof()) {
                io->close();
            }
        }
    }
    if(urls.isEmpty() && (!io || !io->isOpen())) {
        ch->setEnded();
        done = true;
    }
    return NULL;
}

void TextReader::doc2data(Document* doc) {
    algoLog.info(tr("Reading text from %1").arg(doc->getURLString()));
    foreach(GObject* go, GObjectUtils::select(doc->getObjects(), GObjectTypes::TEXT, UOF_LoadedOnly)) {
        TextObject* txtObject = qobject_cast<TextObject*>(go);
        assert(txtObject);
        QVariantMap m; 
        m[BaseSlots::TEXT_SLOT().getId()] = txtObject->getText();
        m[BaseSlots::URL_SLOT().getId()] = doc->getURLString();
        cache << Message(mtype, m);
    }
}

/*************************************
* TextWriter
*************************************/
void TextWriter::data2doc(Document* doc, const QVariantMap& data) {
    QStringList list = data.value(BaseSlots::TEXT_SLOT().getId()).toStringList();
    QString text = list.join("\n");
    TextObject* to = qobject_cast<TextObject*>(GObjectUtils::selectOne(doc->getObjects(), GObjectTypes::TEXT, UOF_LoadedOnly));
    if (!to) {
        to = new TextObject(text, QString("Text %1").arg(++ct));
        doc->addObject(to);
    } else {
        to->setText(to->getText() + "\n" + text);
    }
}

/*************************************
* FastaWriter
*************************************/
void FastaWriter::data2doc(Document* doc, const QVariantMap& data) {
    data2document(doc, data);
}

void FastaWriter::data2document(Document* doc, const QVariantMap& data) {
    DNASequence seq = qVariantValue<DNASequence>(data.value(BaseSlots::DNA_SEQUENCE_SLOT().getId()));
    QString sequenceName = data.value(BaseSlots::FASTA_HEADER_SLOT().getId()).toString();
    if (sequenceName.isEmpty()) {
        sequenceName = seq.getName();
    } else {
        seq.info.insert(DNAInfo::FASTA_HDR, sequenceName);
    }
    if (sequenceName.isEmpty()) {
        sequenceName = QString("unknown sequence %1").arg(doc->getObjects().size());
    }
    if (seq.alphabet && seq.length() != 0 && !doc->findGObjectByName(sequenceName)) {
        algoLog.trace(QString("Adding seq [%1] to FASTA doc %2").arg(sequenceName).arg(doc->getURLString()));
        doc->addObject(new DNASequenceObject(sequenceName, seq));
    }
}

/*************************************
* FastQWriter
*************************************/
void FastQWriter::data2doc(Document* doc, const QVariantMap& data) {
    data2document(doc, data);
}

void FastQWriter::data2document(Document* doc, const QVariantMap& data) {
    DNASequence seq = qVariantValue<DNASequence>(data.value(BaseSlots::DNA_SEQUENCE_SLOT().getId()));
    QString sequenceName = seq.getName(); 
    if (sequenceName.isEmpty()) {
        sequenceName = QString("unknown sequence %1").arg(doc->getObjects().size());
    }
    if (seq.alphabet && seq.length() != 0 && !doc->findGObjectByName(sequenceName)) {
        algoLog.trace(QString("Adding seq [%1] to FASTQ doc %2").arg(sequenceName).arg(doc->getURLString()));
        doc->addObject(new DNASequenceObject(sequenceName, seq));
        algoLog.info( QString("Sequence %1 added to document").arg(sequenceName) );
    }
}

/*************************************
 * RawSeqWriter
 *************************************/
void RawSeqWriter::data2doc(Document* doc, const QVariantMap& data) {
    data2document(doc, data);
}

// same as FastQWriter::data2document
void RawSeqWriter::data2document(Document* doc, const QVariantMap& data) {
    DNASequence seq = qVariantValue<DNASequence>(data.value(BaseSlots::DNA_SEQUENCE_SLOT().getId()));
    QString sequenceName = seq.getName(); 
    if (sequenceName.isEmpty()) {
        sequenceName = QString("unknown sequence %1").arg(doc->getObjects().size());
    }
    if (seq.alphabet && seq.length() != 0 && !doc->findGObjectByName(sequenceName)) {
        algoLog.trace(QString("Adding seq [%1] to Raw sequence document %2").arg(sequenceName).arg(doc->getURLString()));
        if(doc->getDocumentFormat()->isObjectOpSupported(doc, DocumentFormat::DocObjectOp_Add, GObjectTypes::SEQUENCE)) {
            doc->addObject(new DNASequenceObject(sequenceName, seq));
            algoLog.info( QString("Sequence %1 added to document").arg(sequenceName) );
        } else {
            algoLog.error( QString("Cannot add sequence %1 to document" ).arg(sequenceName) );
        }
    }
}

/*************************************
 * GenbankWriter
 *************************************/
void GenbankWriter::data2doc(Document* doc, const QVariantMap& data) {
    data2document(doc, data);
}

void GenbankWriter::data2document(Document* doc, const QVariantMap& data) {
    DNASequence seq = qVariantValue<DNASequence>(data.value(BaseSlots::DNA_SEQUENCE_SLOT().getId()));
    
    /*QStringList order;
    order << DNAInfo::DEFINITION << DNAInfo::ACCESSION << DNAInfo::VERSION;
    order << DNAInfo::PROJECT << DNAInfo::KEYWORDS << DNAInfo::SEGMENT;
    foreach(const QString& key, order) {
        if (seq.info.contains(key)) {
            QVariant v = seq.info.take(key);
            if (!(v.canConvert(QVariant::String)||v.canConvert(QVariant::StringList))) {
                seq.info.remove(key);
            }
        }
    }*/

    QMapIterator<QString, QVariant> it(seq.info);
    while (it.hasNext())
    {
        it.next();
        if ( !(it.value().type() == QVariant::String || it.value().type() == QVariant::StringList) ) {
            seq.info.remove(it.key());
        }
    }


    QString sequenceName = seq.getName(); //data.value(CoreLib::GENBANK_ACN_SLOT_ID).toString();
    QString annotationName;
    if (sequenceName.isEmpty()) {
        int num = doc->findGObjectByType(GObjectTypes::SEQUENCE).size();
        sequenceName = QString("unknown sequence %1").arg(num);
        int featuresNum = doc->findGObjectByType(GObjectTypes::ANNOTATION_TABLE).size();
        annotationName = QString("unknown features %1").arg(featuresNum);
    } else {
        annotationName = sequenceName + " features";
    }

    QList<SharedAnnotationData> atl = QVariantUtils::var2ftl(data.value(BaseSlots::ANNOTATION_TABLE_SLOT().getId()).toList());
    DNASequenceObject* dna = qobject_cast<DNASequenceObject*>(doc->findGObjectByName(sequenceName));
    if (!dna && !seq.isNull()) {
        doc->addObject(dna = new DNASequenceObject(sequenceName, seq));
        algoLog.trace(QString("Adding seq [%1] to GB doc %2").arg(sequenceName).arg(doc->getURLString()));
    }
    if (!atl.isEmpty()) {
        AnnotationTableObject* att = NULL;
        if (dna) {
            QList<GObject*> relAnns = GObjectUtils::findObjectsRelatedToObjectByRole(dna, GObjectTypes::ANNOTATION_TABLE, GObjectRelationRole::SEQUENCE, doc->getObjects(), UOF_LoadedOnly);
            att = relAnns.isEmpty() ? NULL : qobject_cast<AnnotationTableObject*>(relAnns.first());
        }
        if (!att) {
            doc->addObject(att = new AnnotationTableObject(annotationName));
            if (dna) {
                att->addObjectRelation(dna, GObjectRelationRole::SEQUENCE);
            }
            algoLog.trace(QString("Adding features [%1] to GB doc %2").arg(annotationName).arg(doc->getURLString()));
        }
        foreach(SharedAnnotationData sad, atl) {
            att->addAnnotation(new Annotation(sad), QString());
        }
    }
}

/*************************************
* SeqWriter
*************************************/
void SeqWriter::data2doc(Document* doc, const QVariantMap& data){
    if( format == NULL ) {
        return;
    }
    DocumentFormatId fid = format->getFormatId();
    if( fid == BaseDocumentFormats::PLAIN_FASTA ) {
        FastaWriter::data2document( doc, data );
    }
    else if( fid == BaseDocumentFormats::PLAIN_GENBANK ) {
        GenbankWriter::data2document( doc, data );
    } else if ( fid == BaseDocumentFormats::FASTQ) {
        FastQWriter::data2document( doc, data );    
    } else if( fid == BaseDocumentFormats::RAW_DNA_SEQUENCE ) {
        RawSeqWriter::data2document( doc, data );
    } else {
        assert(0);
        ioLog.error(QString("Unknown data format for writing: %1").arg(fid));
    }
}

/*************************************
* MSAWriter
*************************************/
void MSAWriter::data2doc(Document* doc, const QVariantMap& data) {
    MAlignment ma = data.value(BaseSlots::MULTIPLE_ALIGNMENT_SLOT().getId()).value<MAlignment>();
    if (ma.isEmpty()) {
        algoLog.error(tr("Empty alignment passed for writing to %1").arg(doc->getURLString()));
        return; //FIXME
    }
    if (ma.getName().isEmpty()) {
        ma.setName( QString(MA_OBJECT_NAME + "_%1").arg(ct++) );
    }
    doc->addObject(new MAlignmentObject(ma));
}

/*************************************
* DataWorkerFactory
*************************************/
Worker* DataWorkerFactory::createWorker(Actor* a) {
    // TODO: wtf is this??
    //  each actor must have own factory 

    BaseWorker* w = NULL;
    QString protoId = a->getProto()->getId();
    if (CoreLibConstants::READ_TEXT_PROTO_ID == protoId ) {
        TextReader* t = new TextReader(a);
        w = t;
    } 
    else if (CoreLibConstants::WRITE_TEXT_PROTO_ID == protoId) {
        w = new TextWriter(a);
    } 
    else if (CoreLibConstants::WRITE_FASTA_PROTO_ID == protoId) {
        w = new FastaWriter(a);
    }
    else if (CoreLibConstants::WRITE_GENBANK_PROTO_ID == protoId) {
        w = new GenbankWriter(a);
    }
    else if (CoreLibConstants::WRITE_CLUSTAL_PROTO_ID == protoId) {
        w = new MSAWriter(a, BaseDocumentFormats::CLUSTAL_ALN);
    }
    else if (CoreLibConstants::WRITE_STOCKHOLM_PROTO_ID == protoId) {
        w = new MSAWriter(a, BaseDocumentFormats::STOCKHOLM);
    }
    else if (CoreLibConstants::GENERIC_READ_MA_PROTO_ID == protoId) {
        w = new GenericMSAReader(a);
    }
    else if (CoreLibConstants::GENERIC_READ_SEQ_PROTO_ID == protoId) {
        w = new GenericSeqReader(a);
    } 
    else if(CoreLibConstants::WRITE_MSA_PROTO_ID == protoId ) {
        w = new MSAWriter(a);
    }
    else if(CoreLibConstants::WRITE_SEQ_PROTO_ID == protoId ) {
        w = new SeqWriter(a);
    } 
    else if (CoreLibConstants::WRITE_FASTQ_PROTO_ID == protoId ) {
        w = new FastQWriter(a);
    } else {
        assert(0);
    }
    return w;    
}

void DataWorkerFactory::init() {
    DomainFactory* localDomain = WorkflowEnv::getDomainRegistry()->getById(LocalDomainFactory::ID);
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_FASTA_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_GENBANK_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::READ_TEXT_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_TEXT_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::GENERIC_READ_SEQ_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::GENERIC_READ_MA_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_CLUSTAL_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_STOCKHOLM_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_MSA_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_SEQ_PROTO_ID));
    localDomain->registerEntry(new DataWorkerFactory(CoreLibConstants::WRITE_FASTQ_PROTO_ID));
}

} // Workflow namespace
} // U2 namespace
