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

#ifndef _U2_DOCUMENT_MODEL_H_
#define _U2_DOCUMENT_MODEL_H_

#include <U2Core/global.h>
#include <U2Core/GUrl.h>
#include "StateLockableDataModel.h"

#include <U2Core/UnloadedObject.h>

#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtScript>

namespace U2 {

class TaskStateInfo;

class Document;
class GObject;
class DocumentFormat;
class IOAdapterFactory;
class IOAdapter;
class DocumentFormatConstraints;
class GHints;

/** Document loading mode */
enum DocumentLoadMode {
    // load all objects from document
    DocumentLoadMode_Whole,    
    // load single (the first after the current io position) object
    DocumentLoadMode_SingleObject
};

// Additional info about document format
enum DocumentFormatFlag {
    // Document support reading objects from data stream and can detect object boundaries for all object types correctly
    DocumentFormatFlag_SupportStreaming     = 1<<0,
    // Document support writing
    DocumentFormatFlag_SupportWriting       = 1<<1,
    // Document can only contain 1 object: like text, raw sequence or some formats that do not support streaming
    DocumentFormatFlag_SingleObjectFormat   = 1<<2,
    // Document can't be read from packed stream. Used for database files
    DocumentFormatFlag_NoPack               = 1<<3,
    // Document is not fully loaded to memory. Used for database files
    DocumentFormatFlag_DoNotLoadsMemory     = 1<<4
};


typedef QFlags<DocumentFormatFlag> DocumentFormatFlags;
#define DocumentFormatFlags_SW (DocumentFormatFlags(DocumentFormatFlag_SupportStreaming) | DocumentFormatFlag_SupportWriting)
#define DocumentFormatFlags_W1 (DocumentFormatFlags(DocumentFormatFlag_SupportWriting) | DocumentFormatFlag_SingleObjectFormat)



// Result of the format detection algorithm
// Note: High/Very High, Low/Very low selection is the result of the quality of detection algorithm
// For example if detection algorithm is not advanced enough it must not use VeryHigh rating
enum FormatDetectionResult {
    FormatDetection_NotMatched = -10, // format is not matched and can't be parsed at all
    FormatDetection_VeryLowSimilarity = 1, // very low similarity found. Parsing is allowed
    FormatDetection_LowSimilarity = 2, // save as very low, but slightly better, used as extra step in cross-formats differentiation
    FormatDetection_AverageSimilarity = 3, //see above
    FormatDetection_HighSimilarity = 4,//see above
    FormatDetection_VeryHighSimilarity = 5,//see above
    FormatDetection_Matched = 10 // here we 100% sure that we deal with a known and supported format.
};

class U2CORE_EXPORT DocumentFormat: public QObject {
    Q_OBJECT
public:
    static const QString CREATED_NOT_BY_UGENE;
    static const QString MERGED_SEQ_LOCK;

    enum DocObjectOp {
        DocObjectOp_Add,
        DocObjectOp_Remove
    };


    DocumentFormat(QObject* p, DocumentFormatFlags _flags, const QStringList& fileExts = QStringList())
        : QObject(p), formatFlags(_flags),  fileExtensions(fileExts) {}

    /* returns unique document format id */
    virtual DocumentFormatId getFormatId() const = 0;

    /* returns localized name of the format */
    virtual const QString& getFormatName() const = 0;

    /* returns list of usual file extensions for the format
       Example: "fa", "fasta", "gb" ...
    */
    virtual QStringList getSupportedDocumentFileExtensions() const {return fileExtensions;}

    virtual Document* createNewDocument(IOAdapterFactory* io, const GUrl& url, const QVariantMap& hints = QVariantMap());

    /* io - opened IOAdapter.
    * if document format supports streaming reading it must correctly process DocumentLoadMode
    * otherwise, it will load all file from starting position ( default )
    */
    virtual Document* loadDocument(IOAdapter* io, TaskStateInfo& ti, const QVariantMap& hints, 
                                    DocumentLoadMode mode = DocumentLoadMode_Whole) = 0;

    /** A method for compatibility with old code : creates IO adapter and loads document in DocumentLoadMode_Whole  */
    virtual Document* loadDocument(IOAdapterFactory* iof, const GUrl& url, TaskStateInfo& ti, const QVariantMap& hints);

    virtual void storeDocument(Document* d, TaskStateInfo& ts, IOAdapterFactory* io = NULL, const GUrl& newDocURL = GUrl());
    
    /* io - opened IOAdapter
     * so you can store many documents to this file
     */
    virtual void storeDocument( Document* d, TaskStateInfo& ts, IOAdapter* io);
    
    /** Checks if object can be added/removed to the document */
    virtual bool isObjectOpSupported(const Document* d, DocObjectOp op, GObjectType t) const;

    /*
        Returns score rating that indicates that the data supplied is recognized as a valid document format
        Note: Data can contain only first N (~1024) bytes of the file
        The URL value is optional and provided as supplementary option. URL value here can be empty in some special cases.
    */
    virtual FormatDetectionResult checkRawData(const QByteArray& dataPrefix, const GUrl& url = GUrl()) const = 0;
    
    /* Checks that document format satisfies given constraints */ 
    virtual bool checkConstraints(const DocumentFormatConstraints& c) const;
    
    /* Default implementation does nothing */
    virtual void updateFormatSettings(Document* d) const {Q_UNUSED(d);}

    /*
        These object types can be produced by reading documents
        If the format supports write it must support write operation for all the object types it support
    */
    const QSet<GObjectType>& getSupportedObjectTypes() const {return supportedObjectTypes;}

    DocumentFormatFlags getFlags() const {return formatFlags;}

    bool checkFlags(DocumentFormatFlags flagsToCheck) const { return (formatFlags | flagsToCheck) == formatFlags;}

protected:
    DocumentFormatFlags formatFlags;
    QStringList         fileExtensions;
    QSet<GObjectType>   supportedObjectTypes;
};

class DocumentFormatConstraints {
public:
    DocumentFormatConstraints() : flagsToSupport(0), flagsToExclude(0), 
                                checkRawData(false), minDataCheckResult(FormatDetection_VeryLowSimilarity){}

    void clear() {
        flagsToSupport = 0;
        flagsToExclude = 0;
        checkRawData = false;
        rawData.clear();
        minDataCheckResult = FormatDetection_VeryLowSimilarity;
    }
    void addFlagToSupport(DocumentFormatFlag f) {flagsToSupport |= f;}
    void addFlagToExclude(DocumentFormatFlag f) {flagsToExclude |= f;}

    // If 'true' the format supports write operation
    DocumentFormatFlags flagsToSupport;
    DocumentFormatFlags flagsToExclude;
    QSet<GObjectType>   supportedObjectTypes;

    bool                    checkRawData;
    QByteArray              rawData;
    FormatDetectionResult   minDataCheckResult;

};


class U2CORE_EXPORT DocumentFormatRegistry  : public QObject {
    Q_OBJECT
public:
    DocumentFormatRegistry(QObject* p = NULL) : QObject(p) {}

    virtual bool registerFormat(DocumentFormat* dfs) = 0;

    virtual bool unregisterFormat(DocumentFormat* dfs) = 0;

    virtual QList<DocumentFormatId> getRegisteredFormats() const = 0;

    virtual DocumentFormat* getFormatById(DocumentFormatId id) const = 0;

    virtual DocumentFormat* selectFormatByFileExtension(const QString& fileExt) const = 0;

    virtual QList<DocumentFormatId> selectFormats(const DocumentFormatConstraints& c) const = 0;

signals:
    void si_documentFormatRegistered(DocumentFormat*);
    void si_documentFormatUnregistered(DocumentFormat*);
};

enum DocumentModLock {
    DocumentModLock_IO,
    DocumentModLock_USER,
    DocumentModLock_FORMAT_AS_CLASS,
    DocumentModLock_FORMAT_AS_INSTANCE,
    DocumentModLock_UNLOADED_STATE,
    DocumentModLock_NUM_LOCKS
};

class U2CORE_EXPORT Document : public  StateLockableTreeItem {
    Q_OBJECT
    Q_PROPERTY( QString name WRITE setName READ getName )
    Q_PROPERTY( GUrl url WRITE setURL READ getURL )

public:
    class Constraints {
    public:
        Constraints() : stateLocked(TriState_Unknown) {}
        TriState                stateLocked;
        QList<DocumentModLock>  notAllowedStateLocks; // if document contains one of these locks -> it's not matched
        QList<DocumentFormatId> formats;              // document format must be in list to match
        GObjectType             objectTypeToAdd;      // document must be ready to add objects of the specified type
    };


    //Creates document in unloaded state. Populates it with unloaded objects
    Document(DocumentFormat* _df, IOAdapterFactory* _io, const GUrl& _url,
        const QList<UnloadedObjectInfo>& unloadedObjects = QList<UnloadedObjectInfo>(),
        const QVariantMap& hints = QVariantMap(), const QString& instanceModLockDesc = QString());

    //Creates document in loaded state. 
    Document(DocumentFormat* _df, IOAdapterFactory* _io, const GUrl& _url, 
                    const QList<GObject*>& objects, const QVariantMap& hints = QVariantMap(), 
                    const QString& instanceModLockDesc = QString());

    virtual ~Document();

    DocumentFormat* getDocumentFormat() const {return df;}

    DocumentFormatId getDocumentFormatId() const {return df->getFormatId();}

    IOAdapterFactory* getIOAdapterFactory() const {return io;}

    const QList<GObject*>& getObjects() const {return objects;}

    void addObject(GObject* ref);

    void removeObject(GObject* o);

    const QString& getName() const {return name;}
    
    void setName(const QString& newName);

    const GUrl& getURL() const {return url;}

    const QString& getURLString() const {return url.getURLString();}

    void setURL(const GUrl& newUrl);

    void makeClean();

    GObject* findGObjectByName(const QString& name) const;

    QList<GObject*> findGObjectByType(GObjectType t, UnloadedObjectFilter f = UOF_LoadedOnly) const;

    bool isLoaded() const {return modLocks[DocumentModLock_UNLOADED_STATE] == 0;}

    void setLoaded(bool v);

    void loadFrom(const Document* d);

    bool unload();

    Document* clone() const;

    bool checkConstraints(const Constraints& c) const;
    
    GHints* getGHints() const {return ctxState;}

    void setGHints(GHints* state);

    QVariantMap getGHintsMap() const;

    StateLock* getDocumentModLock(DocumentModLock type) const {return modLocks[type];}

    bool hasUserModLock() const {return modLocks[DocumentModLock_USER]!=NULL;}

    void setUserModLock(bool v);

    bool isModified() const { return isTreeItemModified(); }

    static void setupToEngine(QScriptEngine *engine);
private:
    static QScriptValue toScriptValue(QScriptEngine *engine, Document* const &in);
    static void fromScriptValue(const QScriptValue &object, Document* &out);
protected:
    void _removeObject(GObject* o, bool ignoreLocks = false);
    void _addObject(GObject* obj, bool ignoreLocks = false);
    void _addObjectToHierarchy(GObject* obj, bool ignoreLocks = false);

    void initModLocks(const QString& instanceModLockDesc, bool loaded);
    
    void checkUnloadedState() const;
    void checkLoadedState() const;
    void checkUniqueObjectNames() const;
    void addUnloadedObjects(const QList<UnloadedObjectInfo>& info);

    DocumentFormat* const       df;
    IOAdapterFactory* const     io;

    GUrl                url;
    QString             name; /* display name == short pathname, excluding the path */
    QList<GObject*>     objects;
    GHints*             ctxState;
    
    StateLock*          modLocks[DocumentModLock_NUM_LOCKS];

signals:
    void si_urlChanged();
    void si_nameChanged();
    void si_objectAdded(GObject* o);
    void si_objectRemoved(GObject* o);
    void si_loadedStateChanged();
};

//TODO: decide if to use filters or constraints. May be it worth to remove Document::Constraints at all..

class U2CORE_EXPORT DocumentFilter {
public:
    virtual ~DocumentFilter(){};
    virtual bool matches(Document* doc) const = 0;
};

class U2CORE_EXPORT DocumentConstraintsFilter : public DocumentFilter {
public:
    DocumentConstraintsFilter(const Document::Constraints& _c) : constraints(_c){}
    
    virtual bool matches(Document* doc) const {
        return doc->checkConstraints(constraints);
    }

protected:
    Document::Constraints constraints;
};

class U2CORE_EXPORT DocumentMimeData : public QMimeData {
    Q_OBJECT
public:
    static const QString MIME_TYPE;
    DocumentMimeData(Document* obj) : objPtr(obj){};
    QPointer<Document> objPtr;
    bool hasFormat ( const QString & mimeType ) const { return (mimeType == MIME_TYPE);}
    QStringList formats () const {return (QStringList() << MIME_TYPE);}
};

} //namespace
Q_DECLARE_METATYPE(U2::Document*)
#endif


