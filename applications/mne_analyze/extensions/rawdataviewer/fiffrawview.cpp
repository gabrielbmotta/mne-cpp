//=============================================================================================================
/**
 * @file     fiffrawview.cpp
 * @author   Lorenz Esch <lesch@mgh.harvard.edu>;
 *           Lars Debor <Lars.Debor@tu-ilmenau.de>;
 *           Simon Heinke <Simon.Heinke@tu-ilmenau.de>
 * @version  dev
 * @date     July, 2018
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
 * @brief    Definition of FiffRawView Class.
 *
 */

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "fiffrawview.h"

#include "fiffrawviewdelegate.h"
#include <anShared/Model/fiffrawviewmodel.h>


//*************************************************************************************************************
//=============================================================================================================
// Qt INCLUDES
//=============================================================================================================

#include <QTableView>
#include <QVBoxLayout>
#include <QDebug>
#include <QHeaderView>
#include <QScrollBar>

#if defined(USE_OPENGL)
    #include <QGLWidget>
#endif

//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace RAWDATAVIEWEREXTENSION;
using namespace ANSHAREDLIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FiffRawView::FiffRawView(QWidget *parent)
: QWidget(parent)
{
    m_pTableView = new QTableView;

#if defined(USE_OPENGL)
    //Use GPU rendering
    QGLFormat currentFormat = QGLFormat(QGL::SampleBuffers);
    currentFormat.setSamples(10);
    QGLWidget* pGLWidget = new QGLWidget(currentFormat);
    m_pTableView->setViewport(pGLWidget);
#endif

    //set vertical layout
    QVBoxLayout *neLayout = new QVBoxLayout(this);
    neLayout->setContentsMargins(0, 0, 0, 0);

    neLayout->addWidget(m_pTableView);

    //set layouts
    this->setLayout(neLayout);
}


//*************************************************************************************************************

FiffRawView::~FiffRawView()
{
    delete m_pTableView;
}


//*************************************************************************************************************

void FiffRawView::initMVCSettings(const QSharedPointer<FiffRawViewModel>& pModel,
                                  const QSharedPointer<FiffRawViewDelegate>& pDelegate)
{
    m_pTableView->setModel(pModel.data());
    m_pTableView->setItemDelegate(pDelegate.data());

    m_pTableView->setObjectName(QString::fromUtf8("m_pTableView"));
    QSizePolicy sizePolicy3(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizePolicy3.setHorizontalStretch(0);
    sizePolicy3.setVerticalStretch(0);
    sizePolicy3.setHeightForWidth(m_pTableView->sizePolicy().hasHeightForWidth());
    m_pTableView->setSizePolicy(sizePolicy3);
    m_pTableView->setMinimumSize(QSize(0, 0));
    m_pTableView->setMouseTracking(false);
    m_pTableView->setAutoScroll(false);
    m_pTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_pTableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_pTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_pTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_pTableView->setShowGrid(false);
    m_pTableView->horizontalHeader()->setVisible(false);

    //m_pTableView->verticalHeader()->setDefaultSectionSize(m_pRawDelegate->m_iDefaultPlotHeight);
    m_pTableView->setColumnHidden(0,true); //because content is plotted jointly with column=1
    m_pTableView->setColumnHidden(2,true); //because we do not want to plot the mean values

    //Install event filter to overcome QGrabGesture and QScrollBar/QHeader problem
    m_pTableView->horizontalScrollBar()->installEventFilter(this);
    m_pTableView->verticalScrollBar()->installEventFilter(this);
    m_pTableView->verticalHeader()->installEventFilter(this);

    //Enable gestures for the view
    m_pTableView->grabGesture(Qt::PinchGesture);
    m_pTableView->installEventFilter(this);

    //Enable event fitlering for the viewport in order to intercept mouse events
    m_pTableView->viewport()->installEventFilter(this);

    // Connect QScrollBar with model in order to reload data samples
    connect(m_pTableView->horizontalScrollBar(), &QScrollBar::valueChanged,
            pModel.data(), &FiffRawViewModel::updateScrollPosition);

    // Connect and init resizing of the table view to the MVC
    connect(this, &FiffRawView::tableViewDataWidthChanged,
            pModel.data(), &FiffRawViewModel::setDataColumnWidth);

    pModel->setDataColumnWidth(m_pTableView->width()-m_pTableView->columnWidth(0));
    m_pTableView->resizeColumnsToContents();

//    m_pTableView->setColumnHidden(0,true);
//    m_pTableView->setColumnHidden(2,true);
//    m_pTableView->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
//    m_pTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); //Stretch 2 column to maximal width
//    m_pTableView->horizontalHeader()->hide();
//    m_pTableView->resizeColumnsToContents();
//    m_pTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
//    m_pTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

}


//*************************************************************************************************************

void FiffRawView::resizeEvent(QResizeEvent * event)
{
    if(m_pTableView) {
        emit tableViewDataWidthChanged(m_pTableView->width()-m_pTableView->columnWidth(0));
        m_pTableView->resizeColumnsToContents();
    }

    return QWidget::resizeEvent(event);
}
