//=============================================================================================================
/**
 * @file     rtcmne.cpp
 * @author   Lorenz Esch <lesch@mgh.harvard.edu>;
 *           Christoph Dinh <chdinh@nmr.mgh.harvard.edu>
 * @version  dev
 * @date     February, 2013
 *
 * @section  LICENSE
 *
 * Copyright (C) 2013, Lorenz Esch, Christoph Dinh. All rights reserved.
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
 * @brief    Definition of the RtcMne class.
 *
 */

//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "rtcmne.h"

#include "FormFiles/rtcmnesetupwidget.h"

#include <disp/viewers/minimumnormsettingsview.h>

#include <fs/annotationset.h>
#include <fs/surfaceset.h>

#include <fiff/fiff_info.h>

#include <mne/mne_forwardsolution.h>
#include <mne/mne_sourceestimate.h>
#include <mne/mne_epoch_data_list.h>

#include <inverse/minimumNorm/minimumnorm.h>

#include <rtprocessing/rtinvop.h>

#include <scMeas/realtimesourceestimate.h>
#include <scMeas/realtimemultisamplearray.h>
#include <scMeas/realtimecov.h>
#include <scMeas/realtimeevokedset.h>

#include <utils/ioutils.h>

//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QtCore/QtPlugin>
#include <QtConcurrent>
#include <QDebug>

//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace RTCMNEPLUGIN;
using namespace FIFFLIB;
using namespace SCMEASLIB;
using namespace DISPLIB;
using namespace INVERSELIB;
using namespace RTPROCESSINGLIB;
using namespace SCSHAREDLIB;
using namespace IOBUFFER;
using namespace MNELIB;
using namespace FSLIB;
using namespace Eigen;

//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

RtcMne::RtcMne()
: m_pCircularMatrixBuffer(CircularBuffer_Matrix_double::SPtr(new CircularBuffer_Matrix_double(40)))
, m_pCircularEvokedBuffer(CircularBuffer<FIFFLIB::FiffEvoked>::SPtr::create(40))
, m_bEvokedInput(false)
, m_bRawInput(false)
, m_iNumAverages(1)
, m_iDownSample(1)
, m_iTimePointSps(0)
, m_qFileFwdSolution(QCoreApplication::applicationDirPath() + "/MNE-sample-data/MEG/sample/sample_audvis-meg-eeg-oct-6-fwd.fif")
, m_sAtlasDir(QCoreApplication::applicationDirPath() + "/MNE-sample-data/subjects/sample/label")
, m_sSurfaceDir(QCoreApplication::applicationDirPath() + "/MNE-sample-data/subjects/sample/surf")
, m_sAvrType("3")
, m_sMethod("dSPM")
{
}

//=============================================================================================================

RtcMne::~RtcMne()
{
    m_future.waitForFinished();

    if(this->isRunning()) {
        stop();
    }
}

//=============================================================================================================

QSharedPointer<IPlugin> RtcMne::clone() const
{
    QSharedPointer<RtcMne> pRtcMneClone(new RtcMne());
    return pRtcMneClone;
}

//=============================================================================================================

void RtcMne::init()
{
    // Inits
    m_pFwd = MNEForwardSolution::SPtr(new MNEForwardSolution(m_qFileFwdSolution, false, true));
    m_pAnnotationSet = AnnotationSet::SPtr(new AnnotationSet(m_sAtlasDir+"/lh.aparc.a2009s.annot", m_sAtlasDir+"/rh.aparc.a2009s.annot"));
    m_pSurfaceSet = SurfaceSet::SPtr(new SurfaceSet(m_sSurfaceDir+"/lh.inflated", m_sSurfaceDir+"/rh.inflated"));

    // Input
    m_pRTMSAInput = PluginInputData<RealTimeMultiSampleArray>::create(this, "MNE RTMSA In", "MNE real-time multi sample array input data");
    connect(m_pRTMSAInput.data(), &PluginInputConnector::notify,
            this, &RtcMne::updateRTMSA, Qt::DirectConnection);
    m_inputConnectors.append(m_pRTMSAInput);

    m_pRTESInput = PluginInputData<RealTimeEvokedSet>::create(this, "MNE RTE In", "MNE real-time evoked input data");
    connect(m_pRTESInput.data(), &PluginInputConnector::notify,
            this, &RtcMne::updateRTE, Qt::DirectConnection);
    m_inputConnectors.append(m_pRTESInput);

    m_pRTCInput = PluginInputData<RealTimeCov>::create(this, "MNE RTC In", "MNE real-time covariance input data");
    connect(m_pRTCInput.data(), &PluginInputConnector::notify,
            this, &RtcMne::updateRTC, Qt::DirectConnection);
    m_inputConnectors.append(m_pRTCInput);

    // Output
    m_pRTSEOutput = PluginOutputData<RealTimeSourceEstimate>::create(this, "MNE Out", "MNE output data");
    m_outputConnectors.append(m_pRTSEOutput);
    m_pRTSEOutput->data()->setName(this->getName());//Provide name to auto store widget settings

    // Set the fwd, annotation and surface data
    if(m_pAnnotationSet->size() != 0) {
        m_pRTSEOutput->data()->setAnnotSet(m_pAnnotationSet);
    }

    if(m_pSurfaceSet->size() != 0) {
        m_pRTSEOutput->data()->setSurfSet(m_pSurfaceSet);
    }

    if(m_pFwd->nchan != -1) {
        m_pRTSEOutput->data()->setFwdSolution(m_pFwd);
    }
}

//=============================================================================================================

void RtcMne::initPluginControlWidgets()
{
    QList<QWidget*> plControlWidgets;

    MinimumNormSettingsView* pMinimumNormSettingsView = new MinimumNormSettingsView;
    pMinimumNormSettingsView->setObjectName("group_tab_Settings_Source Localization");

    connect(pMinimumNormSettingsView, &MinimumNormSettingsView::methodChanged,
            this, &RtcMne::onMethodChanged);
    connect(pMinimumNormSettingsView, &MinimumNormSettingsView::triggerTypeChanged,
            this, &RtcMne::onTriggerTypeChanged);
    connect(pMinimumNormSettingsView, &MinimumNormSettingsView::timePointChanged,
            this, &RtcMne::onTimePointValueChanged);
    connect(this, &RtcMne::responsibleTriggerTypesChanged,
            pMinimumNormSettingsView, &MinimumNormSettingsView::setTriggerTypes);

    plControlWidgets.append(pMinimumNormSettingsView);

    emit pluginControlWidgetsChanged(plControlWidgets, this->getName());

    m_bPluginControlWidgetsInit = true;
}

//=============================================================================================================

void RtcMne::unload()
{
    m_future.waitForFinished();
}

//=============================================================================================================

void RtcMne::calcFiffInfo()
{
    QMutexLocker locker(&m_qMutex);

    if(m_qListCovChNames.size() > 0 && m_pFiffInfoInput && m_pFiffInfoForward)  {
        qDebug() << "RtcMne::calcFiffInfoFiff - Infos available";

//        qDebug() << "RtcMne::calcFiffInfo - m_qListCovChNames" << m_qListCovChNames;
//        qDebug() << "RtcMne::calcFiffInfo - m_pFiffInfoForward->ch_names" << m_pFiffInfoForward->ch_names;
//        qDebug() << "RtcMne::calcFiffInfo - m_pFiffInfoInput->ch_names" << m_pFiffInfoInput->ch_names;

        // Align channel names of the forward solution to the incoming averaged (currently acquired) data
        // Find out whether the forward solution depends on only MEG, EEG or both MEG and EEG channels
        QStringList forwardChannelsTypes;
        m_pFiffInfoForward->ch_names.clear();
        int counter = 0;

        for(qint32 x = 0; x < m_pFiffInfoForward->chs.size(); ++x) {
            if(forwardChannelsTypes.contains("MEG") && forwardChannelsTypes.contains("EEG"))
                break;

            if(m_pFiffInfoForward->chs[x].kind == FIFFV_MEG_CH && !forwardChannelsTypes.contains("MEG"))
                forwardChannelsTypes<<"MEG";

            if(m_pFiffInfoForward->chs[x].kind == FIFFV_EEG_CH && !forwardChannelsTypes.contains("EEG"))
                forwardChannelsTypes<<"EEG";
        }

        //If only MEG channels are used
        if(forwardChannelsTypes.contains("MEG") && !forwardChannelsTypes.contains("EEG")) {
            for(qint32 x = 0; x < m_pFiffInfoInput->chs.size(); ++x)
            {
                if(m_pFiffInfoInput->chs[x].kind == FIFFV_MEG_CH) {
                    m_pFiffInfoForward->chs[counter].ch_name = m_pFiffInfoInput->chs[x].ch_name;
                    m_pFiffInfoForward->ch_names << m_pFiffInfoInput->chs[x].ch_name;
                    counter++;
                }
            }
        }

        //If only EEG channels are used
        if(!forwardChannelsTypes.contains("MEG") && forwardChannelsTypes.contains("EEG")) {
            for(qint32 x = 0; x < m_pFiffInfoInput->chs.size(); ++x)
            {
                if(m_pFiffInfoInput->chs[x].kind == FIFFV_EEG_CH) {
                    m_pFiffInfoForward->chs[counter].ch_name = m_pFiffInfoInput->chs[x].ch_name;
                    m_pFiffInfoForward->ch_names << m_pFiffInfoInput->chs[x].ch_name;
                    counter++;
                }
            }
        }

        //If both MEG and EEG channels are used
        if(forwardChannelsTypes.contains("MEG") && forwardChannelsTypes.contains("EEG")) {
            //qDebug()<<"RtcMne::calcFiffInfo - MEG EEG fwd solution";
            for(qint32 x = 0; x < m_pFiffInfoInput->chs.size(); ++x)
            {
                if(m_pFiffInfoInput->chs[x].kind == FIFFV_MEG_CH || m_pFiffInfoInput->chs[x].kind == FIFFV_EEG_CH) {
                    m_pFiffInfoForward->chs[counter].ch_name = m_pFiffInfoInput->chs[x].ch_name;
                    m_pFiffInfoForward->ch_names << m_pFiffInfoInput->chs[x].ch_name;
                    counter++;
                }
            }
        }

        //Pick only channels which are present in all data structures (covariance, evoked and forward)
        QStringList tmp_pick_ch_names;
        foreach (const QString &ch, m_pFiffInfoForward->ch_names)
        {
            if(m_pFiffInfoInput->ch_names.contains(ch))
                tmp_pick_ch_names << ch;
        }
        m_qListPickChannels.clear();

        foreach (const QString &ch, tmp_pick_ch_names)
        {
            if(m_qListCovChNames.contains(ch))
                m_qListPickChannels << ch;
        }
        RowVectorXi sel = m_pFiffInfoInput->pick_channels(m_qListPickChannels);

        //qDebug() << "RtcMne::calcFiffInfo - m_qListPickChannels.size()" << m_qListPickChannels.size();
        //qDebug() << "RtcMne::calcFiffInfo - m_qListPickChannels" << m_qListPickChannels;

        m_pFiffInfo = QSharedPointer<FiffInfo>(new FiffInfo(m_pFiffInfoInput->pick_info(sel)));

        m_pRTSEOutput->data()->setFiffInfo(m_pFiffInfo);

        qDebug() << "RtcMne::calcFiffInfo - m_pFiffInfo" << m_pFiffInfo->ch_names;
    }
}

//=============================================================================================================

void RtcMne::doClustering()
{
    if(m_pFwd->nchan == -1) {
        qInfo() << "[RtcMne::doClustering] The forward solution has not been loaded yet.";
        return;
    }

    if(m_pAnnotationSet->size() == 0) {
        qInfo() << "[RtcMne::doClustering] The annotation set has not been loaded yet.";
        return;
    }

    if(m_pFwd->isClustered()) {
        qInfo() << "[RtcMne::doClustering] Forward solution is already clustered.";
        return;
    }

    emit clusteringStarted();

    m_qMutex.lock();
    m_pFwd = MNEForwardSolution::SPtr(new MNEForwardSolution(m_pFwd->cluster_forward_solution(*m_pAnnotationSet.data(), 200)));
    m_pRTSEOutput->data()->setFwdSolution(m_pFwd);

    m_qMutex.unlock();

    finishedClustering();
}

//=============================================================================================================

void RtcMne::finishedClustering()
{
    m_qMutex.lock();
    m_pFiffInfoForward = QSharedPointer<FiffInfoBase>(new FiffInfoBase(m_pFwd->info));
    m_qMutex.unlock();

    emit clusteringFinished();
}

//=============================================================================================================

bool RtcMne::start()
{
    if(m_pFwd->nchan == -1) {
        QMessageBox msgBox;
        msgBox.setText("The forward solution has not been loaded yet.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowFlags(Qt::WindowStaysOnTopHint);
        msgBox.exec();
        return false;
    }

    if(!m_pFwd->isClustered()) {
        QMessageBox msgBox;
        msgBox.setText("The forward solution has not been clustered yet. Please click the Start Clustering button in the Source Localization plugin.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setWindowFlags(Qt::WindowStaysOnTopHint);
        msgBox.exec();
        return false;
    }

    QThread::start();
    return true;
}

//=============================================================================================================

bool RtcMne::stop()
{
    requestInterruption();
    wait(500);

    m_qListCovChNames.clear();
    m_bEvokedInput = false;
    m_bRawInput = false;
    m_bPluginControlWidgetsInit = false;

    return true;
}

//=============================================================================================================

IPlugin::PluginType RtcMne::getType() const
{
    return _IAlgorithm;
}

//=============================================================================================================

QString RtcMne::getName() const
{
    return "Source Localization";
}

//=============================================================================================================

QWidget* RtcMne::setupWidget()
{
    RtcMneSetupWidget* setupWidget = new RtcMneSetupWidget(this);//widget is later distroyed by CentralWidget - so it has to be created everytime new

    connect(this, &RtcMne::clusteringStarted,
            setupWidget, &RtcMneSetupWidget::setClusteringState);
    connect(this, &RtcMne::clusteringFinished,
            setupWidget, &RtcMneSetupWidget::setSetupState);

    return setupWidget;
}

//=============================================================================================================

void RtcMne::updateRTMSA(SCMEASLIB::Measurement::SPtr pMeasurement)
{
    QSharedPointer<RealTimeMultiSampleArray> pRTMSA = pMeasurement.dynamicCast<RealTimeMultiSampleArray>();

    if(pRTMSA && this->isRunning()) {
        //Fiff Information of the RTMSA
        m_qMutex.lock();
        if(!m_pFiffInfoInput) {
            m_pFiffInfoInput = pRTMSA->info();
            m_iNumAverages = 1;
            m_bRawInput = true;
        }
        m_qMutex.lock();

        if(!m_bPluginControlWidgetsInit) {
            initPluginControlWidgets();
        }

        if(this->isRunning()) {
            // Check for artifacts
            QMap<QString,double> mapReject;
            mapReject.insert("eog", 150e-06);

            for(qint32 i = 0; i < pRTMSA->getMultiSampleArray().size(); ++i) {
                bool bArtifactDetected = MNEEpochDataList::checkForArtifact(pRTMSA->getMultiSampleArray()[i],
                                                                            *m_pFiffInfoInput,
                                                                            mapReject);

                if(!bArtifactDetected) {
                    // Please note that we do not need a copy here since this function will block until
                    // the buffer accepts new data again. Hence, the data is not deleted in the actual
                    // Measurement function after it emitted the notify signal.
                    while(!m_pCircularMatrixBuffer->push(pRTMSA->getMultiSampleArray()[i])) {
                        //Do nothing until the circular buffer is ready to accept new data again
                    }
                } else {
                    qDebug() << "RtcMne::updateRTMSA - Reject data block";
                }
            }
        }
    }
}

//=============================================================================================================

void RtcMne::updateRTC(SCMEASLIB::Measurement::SPtr pMeasurement)
{
    QSharedPointer<RealTimeCov> pRTC = pMeasurement.dynamicCast<RealTimeCov>();

    if(pRTC && this->isRunning()) {
        // Init Real-Time inverse estimator
        if(!m_pRtInvOp && m_pFiffInfo && m_pFwd) {
            m_pRtInvOp = RtInvOp::SPtr(new RtInvOp(m_pFiffInfo, m_pFwd));
            connect(m_pRtInvOp.data(), &RtInvOp::invOperatorCalculated,
                    this, &RtcMne::updateInvOp);
        }

        //Fiff Information of the covariance
        if(m_qListCovChNames.size() != pRTC->getValue()->names.size()) {
            m_qListCovChNames = pRTC->getValue()->names;
        }

        if(this->isRunning() && m_pRtInvOp){
            m_pRtInvOp->append(*pRTC->getValue());
        }
    }
}

//=============================================================================================================

void RtcMne::updateRTE(SCMEASLIB::Measurement::SPtr pMeasurement)
{
    if(QSharedPointer<RealTimeEvokedSet> pRTES = pMeasurement.dynamicCast<RealTimeEvokedSet>()) {
        QStringList lResponsibleTriggerTypes = pRTES->getResponsibleTriggerTypes();
        emit responsibleTriggerTypesChanged(lResponsibleTriggerTypes);

        if(!this->isRunning() || !lResponsibleTriggerTypes.contains(m_sAvrType)) {
            return;
        }

        FiffEvokedSet::SPtr pFiffEvokedSet = pRTES->getValue();

        //Fiff Information of the evoked
        if(!m_pFiffInfoInput && pFiffEvokedSet->evoked.size() > 0) {
            QMutexLocker locker(&m_qMutex);

            for(int i = 0; i < pFiffEvokedSet->evoked.size(); ++i) {
                if(pFiffEvokedSet->evoked.at(i).comment == m_sAvrType) {
                    m_pFiffInfoInput = QSharedPointer<FiffInfo>(new FiffInfo(pFiffEvokedSet->evoked.at(i).info));
                    break;
                }
            }

            m_bEvokedInput = true;
        }

        if(!m_bPluginControlWidgetsInit) {
            initPluginControlWidgets();
        }

        if(this->isRunning()) {
            for(int i = 0; i < pFiffEvokedSet->evoked.size(); ++i) {
                if(pFiffEvokedSet->evoked.at(i).comment == m_sAvrType) {
                    // Store current evoked as member so we can dispatch it if the time pick by the user changed
                    m_currentEvoked = pFiffEvokedSet->evoked.at(i).pick_channels(m_qListPickChannels);

                    // Please note that we do not need a copy here since this function will block until
                    // the buffer accepts new data again. Hence, the data is not deleted in the actual
                    // Measurement function after it emitted the notify signal.
                    while(!m_pCircularEvokedBuffer->push(pFiffEvokedSet->evoked.at(i).pick_channels(m_qListPickChannels))) {
                        //Do nothing until the circular buffer is ready to accept new data again
                    }

                    //qDebug()<<"RtcMne::updateRTE - average found type" << m_sAvrType;
                    break;
                }
            }
        }
    }
}

//=============================================================================================================

void RtcMne::updateInvOp(const MNEInverseOperator& invOp)
{
    QMutexLocker locker(&m_qMutex);

    m_invOp = invOp;

    double snr = 1.0;
    double lambda2 = 1.0 / pow(snr, 2); //ToDo estimate lambda using covariance

    m_pMinimumNorm = MinimumNorm::SPtr(new MinimumNorm(m_invOp, lambda2, m_sMethod));

    //Set up the inverse according to the parameters
    // Use 1 nave here because in case of evoked data as input the minimum norm will always be updated when the source estimate is calculated (see run method).
    m_pMinimumNorm->doInverseSetup(1,true);
}

//=============================================================================================================

void RtcMne::onMethodChanged(const QString& method)
{
    m_sMethod = method;

    QMutexLocker locker(&m_qMutex);

    if(m_pMinimumNorm) {
        double snr = 1.0;
        double lambda2 = 1.0 / pow(snr, 2); //ToDo estimate lambda using covariance
        m_pMinimumNorm = MinimumNorm::SPtr(new MinimumNorm(m_invOp, lambda2, m_sMethod));

        // Set up the inverse according to the parameters.
        // Use 1 nave here because in case of evoked data as input the minimum norm will always be updated when the source estimate is calculated (see run method).
        m_pMinimumNorm->doInverseSetup(1,true);
    }
}

//=============================================================================================================

void RtcMne::onTriggerTypeChanged(const QString& triggerType)
{
    m_sAvrType = triggerType;
}

//=============================================================================================================

void RtcMne::onTimePointValueChanged(int iTimePointMs)
{
    if(m_pFiffInfoInput && m_pCircularEvokedBuffer) {
        m_qMutex.lock();
        m_iTimePointSps = m_pFiffInfoInput->sfreq * (float)iTimePointMs * 0.001f;
        m_qMutex.unlock();

        if(this->isRunning()) {
            while(!m_pCircularEvokedBuffer->push(m_currentEvoked)) {
                //Do nothing until the circular buffer is ready to accept new data again
            }
        }
    }
}

//=============================================================================================================

void RtcMne::run()
{
    // Wait for fiff info and minimum norm instance
    while(true) {
        m_qMutex.lock();
        if(m_pFiffInfo && m_pMinimumNorm) {
            m_qMutex.unlock();
            break;
        }
        m_qMutex.unlock();

        if(!m_pFiffInfo) {
            calcFiffInfo();
        }
        msleep(100);
    }

    // Init parameters
    qint32 skip_count = 0;
    FiffEvoked evoked;
    MatrixXd matData;
    MatrixXd matDataResized;
    qint32 j;
    int iTimePointSps = 0;
    float tmin, tstep;
    MNESourceEstimate sourceEstimate;
    bool bEvokedInput = false;
    bool bRawInput = false;

    // Start processing data
    while(!isInterruptionRequested()) {
        m_qMutex.lock();
        iTimePointSps = m_iTimePointSps;
        bEvokedInput = m_bEvokedInput;
        bRawInput = m_bRawInput;
        m_qMutex.unlock();

        //Process data from raw data input
        if(bRawInput) {
            if(((skip_count % m_iDownSample) == 0)) {
                // Get the current raw data
                if(m_pCircularMatrixBuffer->pop(matData)) {
                    //Pick the same channels as in the inverse operator
                    m_qMutex.lock();
                    matDataResized.resize(m_invOp.noise_cov->names.size(), matData.cols());

                    for(j = 0; j < m_invOp.noise_cov->names.size(); ++j) {
                        matDataResized.row(j) = matData.row(m_pFiffInfoInput->ch_names.indexOf(m_invOp.noise_cov->names.at(j)));
                    }

                    tmin = 0.0f;
                    tstep = 1.0f / m_pFiffInfoInput->sfreq;

                    //TODO: Add picking here. See evoked part as input.
                    sourceEstimate = m_pMinimumNorm->calculateInverse(matDataResized,
                                                                      tmin,
                                                                      tstep,
                                                                      true);

                    m_qMutex.unlock();

                    if(!sourceEstimate.isEmpty()) {
                        //qInfo() << QDateTime::currentDateTime().toString("hh:mm:ss.z") << m_iBlockNumberProcessed++ << "MNE Processed";
                        if(iTimePointSps < sourceEstimate.data.cols() && iTimePointSps >= 0) {
                            sourceEstimate = sourceEstimate.reduce(iTimePointSps,1);
                            m_pRTSEOutput->data()->setValue(sourceEstimate);
                        } else {
                            m_pRTSEOutput->data()->setValue(sourceEstimate);
                        }

                        m_pRTSEOutput->data()->setValue(sourceEstimate);
                    }
                }
            } else {
                m_pCircularMatrixBuffer->pop(matDataResized);
            }
        }

        //Process data from averaging input
        if(bEvokedInput) {
            if(m_pCircularEvokedBuffer->pop(evoked)) {
                // Get the current evoked data
                if(((skip_count % m_iDownSample) == 0)) {
    //                    QElapsedTimer time;
    //                    time.start();

                    m_qMutex.lock();
                    sourceEstimate = m_pMinimumNorm->calculateInverse(evoked);
                    m_qMutex.unlock();

                    if(!sourceEstimate.isEmpty()) {
                        //qInfo() << time.elapsed() << m_iBlockNumberProcessed << "MNE Time";
                        //qInfo() << QDateTime::currentDateTime().toString("hh:mm:ss.z") << m_iBlockNumberProcessed++ << "MNE Processed";

                        if(iTimePointSps < sourceEstimate.data.cols() && iTimePointSps >= 0) {
                            sourceEstimate = sourceEstimate.reduce(iTimePointSps,1);
                            m_pRTSEOutput->data()->setValue(sourceEstimate);
                        } else {
                            m_pRTSEOutput->data()->setValue(sourceEstimate);
                        }
                    }
                } else {
                    m_pCircularEvokedBuffer->pop(evoked);
                }
            }
        }
    }

    ++skip_count;
}
