//=============================================================================================================
/**
 * @file     fiffrawviewmodel.cpp
 * @author   Lorenz Esch <lesch@mgh.harvard.edu>;
 *           Lars Debor <Lars.Debor@tu-ilmenau.de>;
 *           Simon Heinke <Simon.Heinke@tu-ilmenau.de>
 * @version  dev
 * @date     October, 2018
 *
 * @section  LICENSE
 *
 * Copyright (C) 2018, Lorenz Esch, Lars Debor, Simon Heinke. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
 *       following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 *       the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of MNE-CPP authors nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @brief    FiffRawViewModel class definition.
 *
 */

//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "fiffrawviewmodel.h"

#include "../Utils/metatypes.h"

#include <fiff/fiff.h>

//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>
#include <QFile>

//=============================================================================================================
// Eigen INCLUDES
//=============================================================================================================

//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace ANSHAREDLIB;
using namespace FIFFLIB;

//=============================================================================================================
// DEFINE GLOBAL METHODS
//=============================================================================================================

//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FiffRawViewModel::FiffRawViewModel(QObject *pParent)
: AbstractModel(pParent)
{
    qDebug() << "[FiffRawViewModel::FiffRawViewModel] Default constructor called !";
}

//=============================================================================================================

FiffRawViewModel::FiffRawViewModel(const QString &sFilePath,
                                   qint32 iVisibleWindowSize,
                                   qint32 iPreloadBufferSize,
                                   QObject *pParent)
: AbstractModel(pParent)
, m_iVisibleWindowSize(iVisibleWindowSize)
, m_iPreloadBufferSize(std::max(2, iPreloadBufferSize))
, m_iSamplesPerBlock(1024)
, m_iTotalBlockCount(m_iVisibleWindowSize + 2 * m_iPreloadBufferSize)
, m_iFiffCursorBegin(-1)
, m_bStartOfFileReached(true)
, m_bEndOfFileReached(false)
, m_blockLoadFutureWatcher()
, m_bCurrentlyLoading(false)
, m_dataMutex()
, m_pFiffIO()
, m_pFiffInfo()
, m_ChannelInfoList()
, m_dDx(1.0)
{
    // connect data reloading: this will be run concurrently
    connect(&m_blockLoadFutureWatcher,
            &QFutureWatcher<int>::finished,
            [this]() {
                postBlockLoad(m_blockLoadFutureWatcher.future().result());
            });

    m_file.setFileName(sFilePath);
    initFiffData(m_file);

    updateEndStartFlags();
}

//=============================================================================================================

FiffRawViewModel::FiffRawViewModel(const QString &sFilePath,
                                   const QByteArray& byteLoadedData,
                                   qint32 iVisibleWindowSize,
                                   qint32 iPreloadBufferSize,
                                   QObject *pParent)
: AbstractModel(pParent)
, m_iVisibleWindowSize(iVisibleWindowSize)
, m_iPreloadBufferSize(std::max(2, iPreloadBufferSize))
, m_iSamplesPerBlock(1024)
, m_iTotalBlockCount(m_iVisibleWindowSize + 2 * m_iPreloadBufferSize)
, m_iFiffCursorBegin(-1)
, m_bStartOfFileReached(true)
, m_bEndOfFileReached(false)
, m_blockLoadFutureWatcher()
, m_bCurrentlyLoading(false)
, m_dataMutex()
, m_pFiffIO()
, m_pFiffInfo()
, m_ChannelInfoList()
, m_dDx(1.0)
{
    // connect data reloading: this will be run concurrently
    connect(&m_blockLoadFutureWatcher,
            &QFutureWatcher<int>::finished,
            [this]() {
                postBlockLoad(m_blockLoadFutureWatcher.future().result());
            });

    m_byteLoadedData = byteLoadedData;
    m_buffer.setData(m_byteLoadedData);

    initFiffData(m_buffer);
    updateEndStartFlags();
}

//=============================================================================================================

FiffRawViewModel::~FiffRawViewModel()
{

}

//=============================================================================================================

void FiffRawViewModel::initFiffData(QIODevice& p_IODevice)
{
    // build FiffIO
    m_pFiffIO = QSharedPointer<FiffIO>::create(p_IODevice);

    if(m_pFiffIO->m_qlistRaw.empty()) {
        qDebug() << "[FiffRawViewModel::loadFiffData] File does not contain any Fiff data";
        return;
    }

    // load channel infos
    for(qint32 i=0; i < m_pFiffIO->m_qlistRaw[0]->info.nchan; ++i)
        m_ChannelInfoList.append(m_pFiffIO->m_qlistRaw[0]->info.chs[i]);

    // load FiffInfo
    m_pFiffInfo = FiffInfo::SPtr(new FiffInfo(m_pFiffIO->m_qlistRaw[0]->info));

    // build datastructure that is to be filled with data from the file
    MatrixXd data, times;

    // append a matrix pair for each block
    for(int i = 0; i < m_iTotalBlockCount; ++i)
        m_lData.push_back(QSharedPointer<QPair<MatrixXd, MatrixXd> >::create(qMakePair(data, times)));

    // Fiff file is not empty, set cursor somewhere into Fiff file
    m_iFiffCursorBegin = m_pFiffIO->m_qlistRaw[0]->first_samp;

    int start = m_iFiffCursorBegin;
    m_iSamplesPerBlock = m_pFiffInfo->sfreq;

    // for some reason the read_raw_segment function works with inclusive upper bound
    int end = start + m_iSamplesPerBlock - 1;

    // read in all blocks, use the already prepared list m_lData
    for(auto &pairPointer : m_lData) {
        if(m_pFiffIO->m_qlistRaw[0]->read_raw_segment(pairPointer->first,
                                                      pairPointer->second,
                                                      start,
                                                      end)) {
            // qDebug() << "[FiffRawmodel::loadFiffData] Successfully read a block ";
        } else {
            qDebug() << "[FiffRawViewModel::loadFiffData] Could not read samples " << start << " to " << end;
            return;
        }

        start += m_iSamplesPerBlock;
        end += m_iSamplesPerBlock;
    }

    qDebug() << "[FiffRawViewModel::initFiffData] Loaded " << m_lData.size() << " blocks";

    // need to close the file manually
    p_IODevice.close();
}

//=============================================================================================================

QVariant FiffRawViewModel::data(const QModelIndex &index, int role) const
{
    // early filtering of unimplemented display roles
    if (role != Qt::DisplayRole) {
        qDebug() << "[FiffRawViewModel] Role " << role << " not implemented yet !";
        return QVariant();
    }

    if (index.isValid()) {
        // channel names
        if(index.column() == 0) {
            return QVariant(m_ChannelInfoList[index.row()].ch_name);
        }

        // whole data
        else if (index.column() == 1) {
            QVariant result;

            switch (role) {
            case Qt::DisplayRole:
                // in order to avoid extensive copying of data, we simply give out smartpointers to the matrices (wrapped inside the ChannelData container)
                // wait until its save to access data (that is if no data insertion is going on right now)
                m_dataMutex.lock();

                // wrap in ChannelData container and then wrap into QVariant
                result.setValue(ChannelData(m_lData, index.row()));

                m_dataMutex.unlock();

                return result;
            }
        }

        // whether channel is marked as bad
        else if(index.column() == 2) {
            return QVariant(m_pFiffInfo->bads.contains(m_pFiffInfo->ch_names[index.row()]));
        }

        else {
            qDebug() << "[FiffRawViewModel] Column " << index.column() << " not implemented !";
            return QVariant();
        }
    }

    qDebug() << "[FiffRawViewModel::data] Warning, non of the presumed cases took effect";
    return QVariant();
}

//=============================================================================================================

QVariant FiffRawViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::TextAlignmentRole)
        return QVariant();

    if(orientation == Qt::Vertical) {
        QModelIndex chname = createIndex(section,0);
        switch(role) {
        case Qt::DisplayRole:
            return QVariant(data(chname).toString());
        }
    }

    return QVariant();
}

//=============================================================================================================

Qt::ItemFlags FiffRawViewModel::flags(const QModelIndex &index) const
{
    // TODO implement stuff
    return QAbstractItemModel::flags(index);
}

//=============================================================================================================

QModelIndex FiffRawViewModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}

//=============================================================================================================

QModelIndex FiffRawViewModel::parent(const QModelIndex &index) const
{
    // TODO implement stuff
    return QModelIndex();
}

//=============================================================================================================

int FiffRawViewModel::rowCount(const QModelIndex &parent) const
{
    if(m_ChannelInfoList.empty() == false)
        return m_ChannelInfoList.size();

    return 0;
}

//=============================================================================================================

int FiffRawViewModel::columnCount(const QModelIndex &parent) const
{
    // TODO implement stuff
    return 3;
}

//=============================================================================================================

bool FiffRawViewModel::hasChildren(const QModelIndex &parent) const
{
    // TODO implement stuff
    return false;
}

//=============================================================================================================

void FiffRawViewModel::updateScrollPosition(qint32 newScrollPosition)
{
    // check if we are currently loading something in the background. This is a rudimentary solution.
    if (m_bCurrentlyLoading) {
        qDebug() << "[FiffRawViewModel::updateScrollPosition] Background operation still pending, try again later...";
        return;
    }

    qint32 targetCursor = (newScrollPosition / m_dDx) + absoluteFirstSample() ;

    if (targetCursor < m_iFiffCursorBegin + (m_iPreloadBufferSize - 1) * m_iSamplesPerBlock
        && m_bStartOfFileReached == false) {
        // time to move the loaded window. Calculate distance in blocks
        qint32 sampleDist = (m_iFiffCursorBegin + (m_iPreloadBufferSize - 1) * m_iSamplesPerBlock) - targetCursor;
        qint32 blockDist = (qint32) ceil(((double) sampleDist) / ((double) m_iSamplesPerBlock));

        if (blockDist >= m_iTotalBlockCount) {
            // we must "jump" to the new cursor ...
            m_iFiffCursorBegin = std::max(absoluteFirstSample(), m_iFiffCursorBegin - blockDist * m_iSamplesPerBlock);

            // ... and load the whole model anew
            //startBackgroundOperation(&FiffRawViewModel::loadLaterBlocks, m_iTotalBlockCount);
            postBlockLoad(loadLaterBlocks(m_iTotalBlockCount));
        } else {
            // there are some blocks in the intersection of the old and the new window that can stay in the buffer:
            // simply load earlier blocks
            //startBackgroundOperation(&FiffRawViewModel::loadEarlierBlocks, blockDist);
            postBlockLoad(loadEarlierBlocks(blockDist));
        }
    } else if (targetCursor + (m_iVisibleWindowSize * m_iSamplesPerBlock) >= m_iFiffCursorBegin + ((m_iPreloadBufferSize + 1) + m_iVisibleWindowSize) * m_iSamplesPerBlock
               && m_bEndOfFileReached == false) {
        // time to move the loaded window. Calculate distance in blocks
        qint32 sampleDist = targetCursor + (m_iVisibleWindowSize * m_iSamplesPerBlock) - (m_iFiffCursorBegin + ((m_iPreloadBufferSize + 1) + m_iVisibleWindowSize) * m_iSamplesPerBlock);
        qint32 blockDist = (qint32) ceil(((double) sampleDist) / ((double) m_iSamplesPerBlock));

        if (blockDist >= m_iTotalBlockCount) {
            // we must "jump" to the new cursor ...
            m_iFiffCursorBegin = std::min(absoluteLastSample() - m_iTotalBlockCount * m_iSamplesPerBlock, m_iFiffCursorBegin +  blockDist * m_iSamplesPerBlock);

            // ... and load the whole model anew
            //startBackgroundOperation(&FiffRawViewModel::loadLaterBlocks, m_iTotalBlockCount);
            postBlockLoad(loadLaterBlocks(m_iTotalBlockCount));
        } else {
            // there are some blocks in the intersection of the old and the new window that can stay in the buffer:
            // simply load later blocks
            //startBackgroundOperation(&FiffRawViewModel::loadLaterBlocks, blockDist);
            postBlockLoad(loadLaterBlocks(blockDist));
        }
    }
}

//=============================================================================================================

void FiffRawViewModel::startBackgroundOperation(int (FiffRawViewModel::*loadFunction)(int), int iBlocksToLoad)
{
    m_bCurrentlyLoading = true;
    QFuture<int> future = QtConcurrent::run(this, loadFunction, iBlocksToLoad);
    m_blockLoadFutureWatcher.setFuture(future);
}

//=============================================================================================================

int FiffRawViewModel::loadEarlierBlocks(qint32 numBlocks)
{
    QElapsedTimer timer;
    timer.start();

    // check if start of file was reached:
    int leftSamples = (m_iFiffCursorBegin - numBlocks * m_iSamplesPerBlock) - absoluteFirstSample();
    if (leftSamples <= 0) {
        qDebug() << "Reached start of file !";
        // see how many blocks we still can load
        int maxNumBlocks = (m_iFiffCursorBegin - absoluteFirstSample()) / m_iSamplesPerBlock;
        qDebug() << "Loading " << maxNumBlocks << " earlier blocks instead of requested " << numBlocks;
        if (maxNumBlocks != 0) {
            numBlocks = maxNumBlocks;
        }
        else {
            // nothing to be done, cant load any more blocks
            // return 0, meaning that this was a loading of earlier blocks
            return 0;
        }
    }

    // we expect m_lNewData to be empty:
    if (m_lNewData.empty() == false) {
        qDebug() << "[FiffRawViewModel::loadEarlierBlocks] FATAL, temporary data storage non empty !";
        return -1;
    }

    // build data structures to be filled from file
    MatrixXd data, times;
    for(int i = 0; i < numBlocks; ++i) {
        m_lNewData.push_back(QSharedPointer<QPair<MatrixXd, MatrixXd> >::create(qMakePair(data, times)));
    }

    // initialize start and end indices
    int start = m_iFiffCursorBegin;
    int end = start + m_iSamplesPerBlock - 1;

    // read data, use the already prepared list m_lNewData
    for(auto &pairPointer : m_lNewData) {
        start -= m_iSamplesPerBlock;
        end -= m_iSamplesPerBlock;
        if(m_pFiffIO->m_qlistRaw[0]->read_raw_segment(pairPointer->first, pairPointer->second, start, end)) {
            // qDebug() << "[FiffRawViewModel::loadFiffData] Successfully read a block ";
        } else {
            qDebug() << "[FiffRawViewModel::loadEarlierBlocks] Could not read block ";
            return -1;
        }
    }

    // adjust fiff cursor
    m_iFiffCursorBegin = start;

    qDebug() << "[TIME] " << ((float) timer.elapsed()) / ((float) numBlocks) << " (per block) [FiffRawViewModel::loadEarlierBlocks]";

    // return 0, meaning that this was a loading of earlier blocks
    return 0;
}

//=============================================================================================================

int FiffRawViewModel::loadLaterBlocks(qint32 numBlocks)
{
    QElapsedTimer timer;
    timer.start();

    // check if end of file is reached:
    int leftSamples = absoluteLastSample() - (m_iFiffCursorBegin + (m_iTotalBlockCount + numBlocks) * m_iSamplesPerBlock);
    if (leftSamples < 0) {
        qDebug() << "Reached end of file !";
        // see how many blocks we still can load
        int maxNumBlocks = (absoluteLastSample() - (m_iFiffCursorBegin + m_iTotalBlockCount * m_iSamplesPerBlock)) / m_iSamplesPerBlock;
        qDebug() << "Loading " << maxNumBlocks << " later blocks instead of requested " << numBlocks;
        if (maxNumBlocks != 0) {
            numBlocks = maxNumBlocks;
        } else {
            // nothing to be done, cant load any more blocks
            // return 1, meaning that this was a loading of later blocks
            return 1;
        }
    }

    // we expect m_lNewData to be empty:
    if (m_lNewData.empty() == false) {
        qDebug() << "[FiffRawViewModel::loadLaterBlocks] FATAL, temporary data storage non empty !";
        return -1;
    }

    // build data structures to be filled from file
    MatrixXd data, times;
    for(int i = 0; i < numBlocks; ++i) {
        m_lNewData.push_back(QSharedPointer<QPair<MatrixXd, MatrixXd> >::create(qMakePair(data, times)));
    }

    // initialize start and end indices
    int start = m_iFiffCursorBegin + m_iTotalBlockCount * m_iSamplesPerBlock;
    int end = start + m_iSamplesPerBlock - 1;

    // read data, use the already prepaired list m_lNewData
    for(auto &pairPointer : m_lNewData) {
        if(m_pFiffIO->m_qlistRaw[0]->read_raw_segment(pairPointer->first, pairPointer->second, start, end)) {
            // qDebug() << "[FiffRawViewModel::loadFiffData] Successfully read a block ";
        } else {
            qDebug() << "[FiffRawViewModel::loadLaterBlocks] Could not read block ";
            return -1;
        }

        start += m_iSamplesPerBlock;
        end += m_iSamplesPerBlock;
    }

    // adjust fiff cursor
    m_iFiffCursorBegin += numBlocks * m_iSamplesPerBlock;

    qDebug() << "[TIME] " << ((float) timer.elapsed()) / ((float) numBlocks) << " (per block) [FiffRawViewModel::loadLaterBlocks]";

    // return 1, meaning that this was a loading of later blocks
    return 1;
}

//=============================================================================================================

void FiffRawViewModel::postBlockLoad(int result)
{
    QElapsedTimer timer;
    timer.start();

    switch(result){
    case -1:
        qDebug() << "[FiffRawViewModel::postBlockLoad] QFuture returned an error: " << result;
        break;
    case 0:
    {
        // insertion of earlier blocks
        int iNewBlocks = m_lNewData.size();

        m_dataMutex.lock();
        for (int i = 0; i < iNewBlocks; ++i) {
            m_lData.push_front(m_lNewData.front());
            m_lNewData.pop_front();
            // @TODO check if this really frees the associated memory
            m_lData.pop_back();
        }
        m_dataMutex.unlock();

        qDebug() << "[TIME] " << timer.elapsed() << " [FiffRawViewModel::postBlockLoad]";
        emit newBlocksLoaded();

        break;
    }
    case 1:
    {
        // insertion of later blocks
        int iNewBlocks = m_lNewData.size();

        m_dataMutex.lock();
        for (int i = 0; i < iNewBlocks; ++i) {
            m_lData.push_back(m_lNewData.front());
            m_lNewData.pop_front();
            m_lData.pop_front();
        }
        m_dataMutex.unlock();

        emit newBlocksLoaded();

        break;
    }
    default:
        qDebug() << "[FiffRawViewModel::postBlockLoad] FATAL Non-intended return value: " << result;
    }

    updateEndStartFlags();
    m_bCurrentlyLoading = false;
}

//=============================================================================================================

void FiffRawViewModel::updateEndStartFlags()
{
    m_bStartOfFileReached = m_iFiffCursorBegin == absoluteFirstSample();
    m_bEndOfFileReached = (m_iFiffCursorBegin + m_iTotalBlockCount * m_iSamplesPerBlock) == absoluteLastSample();
}
