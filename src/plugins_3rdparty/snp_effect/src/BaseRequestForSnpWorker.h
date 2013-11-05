/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2013 UniPro <ugene@unipro.ru>
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

#ifndef _U2_BASE_REQUEST_FOR_SNP_WORKER_
#define _U2_BASE_REQUEST_FOR_SNP_WORKER_

#include <U2Lang/LocalDomain.h>

#include <U2Core/AppContext.h>
#include <U2Core/DataPathRegistry.h>
#include <U2Core/U2Dbi.h>

#include "SnpRequestKeys.h"

namespace U2 {

class RequestForSnpTask;

namespace LocalWorkflow {

class SnpResultCacheItem{
public:
    SnpResultCacheItem(){;}
    
    U2Variant variantId;
    U2DataId featureId;
    QVariantMap result;


};
class BaseRequestForSnpWorker :     public BaseWorker
{
    Q_OBJECT
public:
                                    BaseRequestForSnpWorker( Actor *p );

    void                            init( );
    Task *                          tick( );
    virtual void                    cleanup( );

    static const QString            DB_SEQUENCE_PATH;
    static const QString            DB_FILE;

private slots:
    void                            sl_taskFinished( );
    void                            sl_trackTaskFinished( );

protected:
    virtual QList< QVariantMap>     getInputDataForRequest( const U2Variant& variant, const U2VariantTrack& track, U2Dbi* dataBase );
    virtual QList<SnpResponseKey>   getResultKeys( ) const = 0;

    virtual QString                 getRequestingScriptName( ) const;
    virtual QList<Task *>           createVariationProcessingTasks( const U2Variant &var,
                                        const U2VariantTrack &track, U2Dbi *dbi );

    static QByteArray               getSequenceForVariant( const U2Variant &variant,
                                        const U2VariantTrack &track, U2Dbi *dataBase,
                                        qint64 &sequenceStart );

    IntegralBus *                   inChannel;
    IntegralBus *                   outChannel;

private:
    QString                         getRequestingScriptPath( ) const;
    void                            handleResult( const U2Variant &variant, const U2DataId &featureId,
                                        const QVariantMap &result, U2Dbi *sessionDbi );

    bool                            checkFlushCache( );
    void                            flushCache( );
    void                            clearCache( );
    QMap<U2DataId, QList< SnpResultCacheItem > >    resultCache;
};

} // namespace LocalWorkflow

} // namespace U2

#endif // _U2_BASE_REQUEST_FOR_SNP_WORKER_
