//=============================================================================================================
/**
 * @file     annotationmodel.h
 * @author   Lorenz Esch <lesch@mgh.harvard.edu>
 *           Gabriel Motta <gbmotta@mgh.harvard.edu>
 * @version  dev
 * @date     March, 2020
 *
 * @section  LICENSE
 *
 * Copyright (C) 2020, Christoph Dinh, Lorenz Esch, Gabriel Motta. All rights reserved.
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
 * @brief    Declaration of the AnnotationModel Class.
 *
 */

#ifndef ANSHAREDLIB_ANNOTATIONMODEL_H
#define ANSHAREDLIB_ANNOTATIONMODEL_H

//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "../anshared_global.h"
#include "../Utils/types.h"
#include "abstractmodel.h"

//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QColor>
#include <QAbstractTableModel>

//=============================================================================================================
// Eigen INCLUDES
//=============================================================================================================


//=============================================================================================================
// DEFINE NAMESPACE ANSHAREDLIB
//=============================================================================================================

namespace ANSHAREDLIB {


class ANSHAREDSHARED_EXPORT AnnotationModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    AnnotationModel(QObject* parent = Q_NULLPTR);

    //=========================================================================================================
    bool insertRows(int position, int span, const QModelIndex & parent);
    bool removeRows(int position, int span, const QModelIndex & parent = QModelIndex());
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex & index) const;

    //=========================================================================================================
    /**
     * Gets a list of event types currently held by the model
     *
     * @return Returns a list of event types
     */
    QStringList getEventTypeList() const;

    //=========================================================================================================
    /**
     * Sets saved sample, used to prepare sample to be added to model.
     *
     * @param [in] iSamplePos   sample number to be set
     */
    void setSamplePos(int iSamplePos);

    //=========================================================================================================
    /**
     * Sets current filter setting sto only display selected annotation type
     *
     * @param [in] eventType    Type of annotation that is to be displayed when filterd
     */
    void setEventFilterType(const QString eventType);

    //=========================================================================================================
    /**
     * Used to pass first and last sample parameters to the model
     *
     * @param [in] firstSample  sample number of the first sample in the currently loaded fiff file
     * @param [in] lastSample   sample number of the last sample in the currently loaded fiff file
     */
    void setFirstLastSample(int firstSample,
                            int lastSample);

    //=========================================================================================================
    /**
     * returns the first and last sample parameters currently stored in the model
     *
     * @return Returns first and last sample parameters
     */
    QPair<int, int> getFirstLastSample() const;

    //=========================================================================================================
    /**
     * Returns frequency parameter stored in the model
     *
     * @return frequency of the samples
     */
    float getSampleFreq() const;

    //=========================================================================================================
    /**
     * Sets the stored frequerncy parameter to the function input
     *
     * @param [in] fFreq    frequency of the currently loaded fiff file
     */
    void setSampleFreq(float fFreq);

    //=========================================================================================================
    /**
     * Return number of annotations to be displayed, based on current filter parameters
     *
     * @return number on annotations to display
     */
    int getNumberOfAnnotations() const;

    //=========================================================================================================
    /**
     * Return annotation stored at index given by input parameter
     *
     * @param [in] iIndex   Index of the annotation to be retreived
     *
     * @return Returns annotation at index given by input parameter
     */
    int getAnnotation(int iIndex) const;

    //=========================================================================================================
    /**
     * Returns map of the colors assigned to each of the annotation types
     *
     * @return Map of annotation colors
     */
    QMap<int, QColor>& getTypeColors();

    //=========================================================================================================
    /**
     * Adds a new annotation type with the input parameters as configuration parameters
     *
     * @param [in] eventType    type number (0-99)
     * @param [in] typeColor    color to be used for drawing
     */
    void addNewAnnotationType(const QString &eventType,
                              const QColor &typeColor);

    //=========================================================================================================
    /**
     * Pass which annotations are currenlty selected in the view GUI
     *
     * @param [in] iSelected    currently selected annotation
     */
    void setSelectedAnn(int iSelected);

    //=========================================================================================================
    /**
     * Returns currently selected annotation stored locally in the model
     *
     * @return Returns stored selected annotation
     */
    int getSelectedAnn();

    //=========================================================================================================
    /**
     * Sets whether only to show selected annotations
     *
     * @param [in] iSelectedState   whether to show only selected. 0 - no, 2 - yes
     */
    void setShowSelected(int iSelectedState);

    //=========================================================================================================
    /**
     * Returns whther to only show selected annotations
     *
     * @return whther to show selected annotations
     */
    int getShowSelected();

    //=========================================================================================================
    /**
     * Returns sample freqency
     *
     * @return sample frequency
     */
    float getFreq();

    //=========================================================================================================
    /**
     * Saves model to the current model path if possible.
     *
     * @param [in] sPath   The path where the file should be saved to.
     *
     * @returns      True if saving was successful
     */
    bool saveToFile(const QString& sPath);

    //=========================================================================================================
    /**
     * Saves last added type or last type to be filterd to
     *
     * @param [in] iType    type to be saved
     */
    void setLastType(int iType);

signals:

    //=========================================================================================================
    /**
     * Emits updated new type to be added to GUI
     *
     * @param [in] currentFilterType    Type to be updated in GUI
     */
    void updateEventTypes(const QString& currentFilterType);

private:

    QStringList         m_eventTypeList;

    QVector<int>        m_dataSamples;
    QVector<int>        m_dataTypes;
    QVector<int>        m_dataIsUserEvent;

    QVector<int>        m_dataSamples_Filtered;
    QVector<int>        m_dataTypes_Filtered;
    QVector<int>        m_dataIsUserEvent_Filtered;

    int                 m_iSamplePos;
    int                 m_iFirstSample;
    int                 m_iLastSample;

    int                 m_iActiveCheckState;
    int                 m_iSelectedCheckState;
    int                 m_iSelectedAnn;

    int                 m_iLastTypeAdded;

    float               m_fFreq;

    QString             m_sFilterEventType;
    QMap<int, QColor>   m_eventTypeColor;

};
}
#endif // ANSHAREDLIB_ANNOTATIONMODEL_H
