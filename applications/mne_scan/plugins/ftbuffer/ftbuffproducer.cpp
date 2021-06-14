//=============================================================================================================
/**
 * @file     ftbuffproducer.cpp
 * @author   Gabriel B Motta <gbmotta@mgh.harvard.edu>;
 *           Lorenz Esch <lorenz.esch@tu-ilmenau.de>
 * @since    0.1.0
 * @date     January, 2020
 *
 * @section  LICENSE
 *
 * Copyright (C) 2020, Gabriel B Motta, Lorenz Esch. All rights reserved.
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
 * @brief    Definition of the FtBuffProducer class.
 *
 */

//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "ftbuffproducer.h"
#include "ftbuffer.h"

//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

//=============================================================================================================
// EIGEN INCLUDES
//=============================================================================================================

//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace FTBUFFERPLUGIN;

//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FtBuffProducer::FtBuffProducer(FtBuffer* pFtBuffer)
: m_pFtBuffer(pFtBuffer)
{
}

//=============================================================================================================

FtBuffProducer::~FtBuffProducer()
{
}

//=============================================================================================================

void FtBuffProducer::interfaceWithBuffer()
{
    while(!this->thread()->isInterruptionRequested()) {
        connectToBuffer();
        parseHeader();
        while(!this->thread()->isInterruptionRequested() && m_bConnectAndHeader){
            getBufferData();
        }
    }
    disconnectFromBuffer();
}

//=============================================================================================================

void FtBuffProducer::connectToBuffer()
{
    emit connecStatus(false);
    m_pFtConnector->setAddr(m_sBufferAddress);
    m_pFtConnector->setPort(m_iBufferPort);

    while(!m_pFtConnector->connect() && !this->thread()->isInterruptionRequested()) {
        QThread::msleep(100);
    }

    emit connecStatus(true);
}

//=============================================================================================================

void FtBuffProducer::parseHeader()
{
    while(!m_pFtConnector->getHeader() && !this->thread()->isInterruptionRequested()) {
        QThread::msleep(100);
    }

    FIFFLIB::FiffInfo info = m_pFtConnector->parseBufferHeaders();
    if(info.sfreq > 0){
        emit newInfoAvailable(info);
    }
}

//=============================================================================================================

void FtBuffProducer::getBufferData()
{
    m_pFtConnector->getData();

    //Sends up new data when FtConnector flags new data
    if (m_pFtConnector->newData()) {
        emit newDataAvailable(m_pFtConnector->getMatrix());
        m_pFtConnector->resetEmitData();
    }
}

//=============================================================================================================

void FtBuffProducer::initConnector()
{
    m_pFtConnector = std::make_unique<FtConnector>();
}

//=============================================================================================================

void FtBuffProducer::startProducer()
{
    initConnector();
    interfaceWithBuffer();
    this->thread()->quit();
}

//=============================================================================================================

void FtBuffProducer::disconnectFromBuffer()
{
    m_pFtConnector->disconnectFromBuffer();
    emit connecStatus(false);
}
