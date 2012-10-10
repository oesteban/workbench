
/*LICENSE_START*/
/*
 * Copyright 2012 Washington University,
 * All rights reserved.
 *
 * Connectome DB and Connectome Workbench are part of the integrated Connectome 
 * Informatics Platform.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the names of Washington University nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR  
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*LICENSE_END*/

#define __FIBER_ORIENTATION_SELECTION_VIEW_CONTROLLER_DECLARE__
#include "FiberOrientationSelectionViewController.h"
#undef __FIBER_ORIENTATION_SELECTION_VIEW_CONTROLLER_DECLARE__

#include <limits>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "Brain.h"
#include "BrowserTabContent.h"
#include "ConnectivityLoaderFile.h"
#include "DisplayGroupEnumComboBox.h"
#include "DisplayPropertiesFiberOrientation.h"
#include "EventManager.h"
#include "EventGraphicsUpdateAllWindows.h"
#include "EventUserInterfaceUpdate.h"
#include "CiftiFiberOrientationAdapter.h"
#include "FiberOrientationColoringTypeEnum.h"
#include "FiberOrientationSelectionViewController.h"
#include "FiberSamplesOpenGLWidget.h"
#include "GuiManager.h"
#include "WuQTabWidget.h"
#include "WuQtUtilities.h"

using namespace caret;


    
/**
 * \class caret::FiberOrientationSelectionViewController 
 * \brief View/Controller for fiber orientations
 */

/**
 * Constructor.
 */
FiberOrientationSelectionViewController::FiberOrientationSelectionViewController(const int32_t browserWindowIndex,
                                                                                 QWidget* parent)
: QWidget(parent),
  m_browserWindowIndex(browserWindowIndex)
{
    QLabel* groupLabel = new QLabel("Group");
    m_displayGroupComboBox = new DisplayGroupEnumComboBox(this);
    QObject::connect(m_displayGroupComboBox, SIGNAL(displayGroupSelected(const DisplayGroupEnum::Enum)),
                     this, SLOT(displayGroupSelected(const DisplayGroupEnum::Enum)));
    
    QHBoxLayout* groupLayout = new QHBoxLayout();
    WuQtUtilities::setLayoutMargins(groupLayout, 2, 2);
    groupLayout->addWidget(groupLabel);
    groupLayout->addWidget(m_displayGroupComboBox->getWidget());
    groupLayout->addStretch();
    
    QWidget* attributesWidget = this->createAttributesWidget();
    QWidget* selectionWidget = this->createSelectionWidget();
    QWidget* samplesWidget   = this->createSamplesWidget();
    
    
    //QTabWidget* tabWidget = new QTabWidget();
    WuQTabWidget* tabWidget = new WuQTabWidget(WuQTabWidget::TAB_ALIGN_LEFT,
                                               this);
    tabWidget->addTab(attributesWidget,
                      "Attributes");
    tabWidget->addTab(selectionWidget,
                      "Files");
    tabWidget->addTab(samplesWidget,
                      "Samples");
    tabWidget->setCurrentWidget(attributesWidget);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    WuQtUtilities::setLayoutMargins(layout, 2, 2);
    layout->addLayout(groupLayout);
    layout->addWidget(tabWidget->getWidget(), 0, Qt::AlignLeft);
    layout->addStretch();
    
    EventManager::get()->addEventListener(this,
                                          EventTypeEnum::EVENT_USER_INTERFACE_UPDATE);
    
    s_allViewControllers.insert(this);
}

/**
 * Destructor.
 */
FiberOrientationSelectionViewController::~FiberOrientationSelectionViewController()
{
    EventManager::get()->removeAllEventsFromListener(this);
    
    s_allViewControllers.erase(this);
}

/**
 * @return The selection widget.
 */
QWidget*
FiberOrientationSelectionViewController::createSelectionWidget()
{
    m_selectionWidgetLayout = new QVBoxLayout();
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->addLayout(m_selectionWidgetLayout);
    layout->addStretch();
    return widget;
}

/**
 * @return The attributes widget.
 */
QWidget*
FiberOrientationSelectionViewController::createAttributesWidget()
{
    m_updateInProgress = true;
    
    m_displayFibersCheckBox = new QCheckBox("Display Fibers");
    m_displayFibersCheckBox->setToolTip("Display Fibers");
    QObject::connect(m_displayFibersCheckBox, SIGNAL(clicked(bool)),
                     this, SLOT(processAttributesChanges()));
    
    QLabel* aboveLimitLabel = new QLabel("Slice Above Limit");
    m_aboveLimitSpinBox = new QDoubleSpinBox();
    m_aboveLimitSpinBox->setRange(0.0, std::numeric_limits<float>::max());
    m_aboveLimitSpinBox->setDecimals(2);
    m_aboveLimitSpinBox->setSingleStep(0.1);
    m_aboveLimitSpinBox->setToolTip("Fibers within this distance above the volume slice will be displayed");
    QObject::connect(m_aboveLimitSpinBox, SIGNAL(valueChanged(double)),
                     this, SLOT(processAttributesChanges()));
    
    QLabel* belowLimitLabel = new QLabel("Slice Below Limit");
    m_belowLimitSpinBox = new QDoubleSpinBox();
    m_belowLimitSpinBox->setRange(-std::numeric_limits<float>::max(), 0.0);
    m_belowLimitSpinBox->setDecimals(2);
    m_belowLimitSpinBox->setSingleStep(0.1);
    m_belowLimitSpinBox->setToolTip("Fibers within this distance below the volume slice will be displayed");
    QObject::connect(m_belowLimitSpinBox, SIGNAL(valueChanged(double)),
                     this, SLOT(processAttributesChanges()));
    
    QLabel* minimumMagnitudeLabel = new QLabel("Minimum Magnitude");
    m_minimumMagnitudeSpinBox = new QDoubleSpinBox();
    m_minimumMagnitudeSpinBox->setRange(0.0, std::numeric_limits<float>::max());
    m_minimumMagnitudeSpinBox->setDecimals(2);
    m_minimumMagnitudeSpinBox->setSingleStep(0.05);
    m_minimumMagnitudeSpinBox->setToolTip("Minimum magnitude for displaying fibers");
    QObject::connect(m_minimumMagnitudeSpinBox, SIGNAL(valueChanged(double)),
                     this, SLOT(processAttributesChanges()));
    
    QLabel* lengthMultiplierLabel = new QLabel("Length Multiplier");
    m_lengthMultiplierSpinBox = new QDoubleSpinBox();
    m_lengthMultiplierSpinBox->setRange(0.0, std::numeric_limits<float>::max());
    m_lengthMultiplierSpinBox->setDecimals(2);
    m_lengthMultiplierSpinBox->setSingleStep(1.0);
    m_lengthMultiplierSpinBox->setToolTip("Fiber lengths are scaled by this value");
    QObject::connect(m_lengthMultiplierSpinBox, SIGNAL(valueChanged(double)),
                     this, SLOT(processAttributesChanges()));
    
    QLabel* fanMultiplierLabel = new QLabel("Fan Multiplier");
    m_fanMultiplierSpinBox = new QDoubleSpinBox();
    m_fanMultiplierSpinBox->setRange(0.0, std::numeric_limits<float>::max());
    m_fanMultiplierSpinBox->setDecimals(2);
    m_fanMultiplierSpinBox->setSingleStep(0.05);
    m_fanMultiplierSpinBox->setToolTip("Fan angles are scaled by this value");
    QObject::connect(m_fanMultiplierSpinBox, SIGNAL(valueChanged(double)),
                     this, SLOT(processAttributesChanges()));
    
    m_drawWithMagnitudeCheckBox = new QCheckBox("Draw With Magnitude");
    m_drawWithMagnitudeCheckBox->setToolTip("When drawing fibers, the magnitude is reflected in the length of the fiber");
    QObject::connect(m_drawWithMagnitudeCheckBox, SIGNAL(clicked(bool)),
                     this, SLOT(processAttributesChanges()));
    
    QLabel* coloringTypeLabel = new QLabel("Coloring");
    m_coloringTypeComboBox = new EnumComboBoxTemplate(this);
    m_coloringTypeComboBox->getWidget()->setToolTip("Selects method for assigning the red, green, and blue\n"
                                                    "color components when drawing fibers");
    QObject::connect(m_coloringTypeComboBox, SIGNAL(itemSelected()),
                     this, SLOT(processAttributesChanges()));
    m_coloringTypeComboBox->setup<FiberOrientationColoringTypeEnum, FiberOrientationColoringTypeEnum::Enum>();
    
    QLabel* symbolTypeLabel = new QLabel("Symbol");
    m_symbolTypeComboBox = new EnumComboBoxTemplate(this);
    m_symbolTypeComboBox->getWidget()->setToolTip("Selects type of symbol for drawing fibers");
    QObject::connect(m_symbolTypeComboBox, SIGNAL(itemSelected()),
                     this, SLOT(processAttributesChanges()));
    m_symbolTypeComboBox->setup<FiberOrientationSymbolTypeEnum, FiberOrientationSymbolTypeEnum::Enum>();
    
    QWidget* gridWidget = new QWidget();
    QGridLayout* gridLayout = new QGridLayout(gridWidget);
    gridLayout->setColumnStretch(0, 0);
    gridLayout->setColumnStretch(1, 100);
    WuQtUtilities::setLayoutMargins(gridLayout, 8, 2);
    int row = gridLayout->rowCount();
    gridLayout->addWidget(m_displayFibersCheckBox, row, 0, 1, 2, Qt::AlignLeft);
    row++;
    gridLayout->addWidget(coloringTypeLabel, row, 0);
    gridLayout->addWidget(m_coloringTypeComboBox->getWidget() , row, 1);
    row++;
    gridLayout->addWidget(symbolTypeLabel, row, 0);
    gridLayout->addWidget(m_symbolTypeComboBox->getWidget() , row, 1);
    row++;
    gridLayout->addWidget(WuQtUtilities::createHorizontalLineWidget(), row, 0, 1, 2);
    row++;
    gridLayout->addWidget(m_drawWithMagnitudeCheckBox, row, 0, 1, 2, Qt::AlignLeft);
    row++;
    gridLayout->addWidget(minimumMagnitudeLabel, row, 0);
    gridLayout->addWidget(m_minimumMagnitudeSpinBox , row, 1);
    row++;
    gridLayout->addWidget(lengthMultiplierLabel, row, 0);
    gridLayout->addWidget(m_lengthMultiplierSpinBox , row, 1);
    row++;
    gridLayout->addWidget(fanMultiplierLabel, row, 0);
    gridLayout->addWidget(m_fanMultiplierSpinBox , row, 1);
    row++;
    gridLayout->addWidget(WuQtUtilities::createHorizontalLineWidget(), row, 0, 1, 2);
    row++;
    gridLayout->addWidget(aboveLimitLabel, row, 0);
    gridLayout->addWidget(m_aboveLimitSpinBox, row, 1);
    row++;
    gridLayout->addWidget(belowLimitLabel, row, 0);
    gridLayout->addWidget(m_belowLimitSpinBox, row, 1);
    row++;
    
    gridWidget->setSizePolicy(QSizePolicy::Fixed,
                              QSizePolicy::Fixed);

    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);
    WuQtUtilities::setLayoutMargins(layout, 2, 2);
    layout->addWidget(gridWidget);
    layout->addStretch();
    
    m_updateInProgress = false;
    
    return widget;
}

/**
 * Called when a widget on the attributes page has
 * its value changed.
 */
void
FiberOrientationSelectionViewController::processAttributesChanges()
{
    if (m_updateInProgress) {
        return;
    }
    
    DisplayPropertiesFiberOrientation* dpfo = GuiManager::get()->getBrain()->getDisplayPropertiesFiberOrientation();
    
    BrowserTabContent* browserTabContent =
    GuiManager::get()->getBrowserTabContentForBrowserWindow(m_browserWindowIndex, true);
    if (browserTabContent == NULL) {
        return;
    }
    const int32_t browserTabIndex = browserTabContent->getTabNumber();
    const DisplayGroupEnum::Enum displayGroup = dpfo->getDisplayGroupForTab(browserTabIndex);
    
    dpfo->setDisplayed(displayGroup,
                       browserTabIndex,
                       m_displayFibersCheckBox->isChecked());
    
    dpfo->setAboveLimit(displayGroup,
                       browserTabIndex,
                       m_aboveLimitSpinBox->value());
    
    dpfo->setBelowLimit(displayGroup,
                        browserTabIndex,
                        m_belowLimitSpinBox->value());
    
    dpfo->setMinimumMagnitude(displayGroup,
                        browserTabIndex,
                        m_minimumMagnitudeSpinBox->value());
    
    dpfo->setLengthMultiplier(displayGroup,
                        browserTabIndex,
                        m_lengthMultiplierSpinBox->value());
    
    dpfo->setFanMultiplier(displayGroup,
                           browserTabIndex,
                           m_fanMultiplierSpinBox->value());
    
    dpfo->setDrawWithMagnitude(displayGroup,
                               browserTabIndex,
                               m_drawWithMagnitudeCheckBox->isChecked());

    const FiberOrientationColoringTypeEnum::Enum coloringType = m_coloringTypeComboBox->getSelectedItem<FiberOrientationColoringTypeEnum, FiberOrientationColoringTypeEnum::Enum>();
    dpfo->setColoringType(displayGroup,
                          browserTabIndex,
                          coloringType);
    
    const FiberOrientationSymbolTypeEnum::Enum symbolType = m_symbolTypeComboBox->getSelectedItem<FiberOrientationSymbolTypeEnum, FiberOrientationSymbolTypeEnum::Enum>();
    dpfo->setSymbolType(displayGroup,
                          browserTabIndex,
                          symbolType);
    
    dpfo->setSphereOrientationsDisplayed(displayGroup,
                                         browserTabIndex,
                                         m_displaySphereOrientationsCheckBox->isChecked());
    
    EventManager::get()->sendEvent(EventGraphicsUpdateAllWindows().getPointer());
    
    updateOtherViewControllers();
}

/**
 * Called when the fiber orientation display group combo box is changed.
 */
void
FiberOrientationSelectionViewController::displayGroupSelected(const DisplayGroupEnum::Enum displayGroup)
{
    if (m_updateInProgress) {
        return;
    }
    
    /*
     * Update selected display group in model.
     */
    BrowserTabContent* browserTabContent =
    GuiManager::get()->getBrowserTabContentForBrowserWindow(m_browserWindowIndex, false);
    if (browserTabContent == NULL) {
        return;
    }
    
    const int32_t browserTabIndex = browserTabContent->getTabNumber();
    Brain* brain = GuiManager::get()->getBrain();
    DisplayPropertiesFiberOrientation* dpfo = brain->getDisplayPropertiesFiberOrientation();
    dpfo->setDisplayGroupForTab(browserTabIndex,
                               displayGroup);
    
    /*
     * Since display group has changed, need to update controls
     */
    updateViewController();
    
    /*
     * Apply the changes.
     */
    processSelectionChanges();
}

/**
 * Update the fiber orientation widget.
 */
void
FiberOrientationSelectionViewController::updateViewController()
{
    m_updateInProgress = true;
    
    BrowserTabContent* browserTabContent =
    GuiManager::get()->getBrowserTabContentForBrowserWindow(m_browserWindowIndex, true);
    if (browserTabContent == NULL) {
        return;
    }
    
    const int32_t browserTabIndex = browserTabContent->getTabNumber();
    Brain* brain = GuiManager::get()->getBrain();
    DisplayPropertiesFiberOrientation* dpfo = brain->getDisplayPropertiesFiberOrientation();

    const DisplayGroupEnum::Enum displayGroup = dpfo->getDisplayGroupForTab(browserTabIndex);
    m_displayGroupComboBox->setSelectedDisplayGroup(displayGroup);
    
    //dpfo->isDisplayed(displayGroup, browserTabIndex)
    
    /*
     * Update file selection checkboxes
     */
    const int32_t numberOfFileCheckBoxes = static_cast<int32_t>(m_fileSelectionCheckBoxes.size());
    std::vector<ConnectivityLoaderFile*> allFiberOrientFiles;
    const int32_t numberOfFiberOrientFiles = brain->getNumberOfConnectivityFiberOrientationFiles();
    for (int32_t iff = 0; iff < numberOfFiberOrientFiles; iff++) {
        ConnectivityLoaderFile* clf = brain->getConnectivityFiberOrientationFile(iff);
        CiftiFiberOrientationAdapter* foca = clf->getFiberOrientationAdapter();
        if (foca != NULL) {
            QCheckBox* cb = NULL;
            if (iff < numberOfFileCheckBoxes) {
                cb = m_fileSelectionCheckBoxes[iff];
            }
            else {
                cb = new QCheckBox("");
                QObject::connect(cb, SIGNAL(clicked(bool)),
                                 this, SLOT(processSelectionChanges()));
                m_fileSelectionCheckBoxes.push_back(cb);
                m_selectionWidgetLayout->addWidget(cb);
            }
            cb->setText(clf->getFileNameNoPath());
            cb->setChecked(foca->isDisplayed(displayGroup,
                                             browserTabIndex));
            cb->setVisible(true);
        }
    }
    
    /*
     * Hide unused file selection checkboxes
     */
    for (int32_t iff = numberOfFiberOrientFiles; iff < numberOfFileCheckBoxes; iff++) {
        m_fileSelectionCheckBoxes[iff]->setVisible(false);
    }
    
    
    /*
     * Update the attributes
     */
    m_displayFibersCheckBox->setChecked(dpfo->isDisplayed(displayGroup,
                                                        browserTabIndex));
    m_aboveLimitSpinBox->setValue(dpfo->getAboveLimit(displayGroup,
                                                      browserTabIndex));
    m_belowLimitSpinBox->setValue(dpfo->getBelowLimit(displayGroup,
                                                      browserTabIndex));
    m_lengthMultiplierSpinBox->setValue(dpfo->getLengthMultiplier(displayGroup,
                                                      browserTabIndex));
    m_fanMultiplierSpinBox->setValue(dpfo->getFanMultiplier(displayGroup,
                                                            browserTabIndex));
    m_minimumMagnitudeSpinBox->setValue(dpfo->getMinimumMagnitude(displayGroup,
                                                      browserTabIndex));
    m_drawWithMagnitudeCheckBox->setChecked(dpfo->isDrawWithMagnitude(displayGroup,
                                                                     browserTabIndex));
    m_coloringTypeComboBox->setSelectedItem<FiberOrientationColoringTypeEnum, FiberOrientationColoringTypeEnum::Enum>(dpfo->getColoringType(displayGroup,
                                                                                                                                             browserTabIndex));
    m_symbolTypeComboBox->setSelectedItem<FiberOrientationSymbolTypeEnum, FiberOrientationSymbolTypeEnum::Enum>(dpfo->getSymbolType(displayGroup,
                                                                                                                                            browserTabIndex));
    m_displaySphereOrientationsCheckBox->setChecked(dpfo->isSphereOrientationsDisplayed(displayGroup,
                                                                                        browserTabIndex));
    m_updateInProgress = false;
}

/**
 * Issue update events after selections are changed.
 */
void
FiberOrientationSelectionViewController::processSelectionChanges()
{
    if (m_updateInProgress) {
        return;
    }

    BrowserTabContent* browserTabContent =
    GuiManager::get()->getBrowserTabContentForBrowserWindow(m_browserWindowIndex, true);
    if (browserTabContent == NULL) {
        return;
    }
    
    Brain* brain = GuiManager::get()->getBrain();
    const int32_t browserTabIndex = browserTabContent->getTabNumber();
    DisplayPropertiesFiberOrientation* dpfo = brain->getDisplayPropertiesFiberOrientation();
    
    const DisplayGroupEnum::Enum displayGroup = dpfo->getDisplayGroupForTab(browserTabIndex);
    //dpfo->setDisplayed(displayGroup, browserTabIndex, m_fileSelectionCheckBoxes[iff]->isChecked());
    
    const int32_t numberOfFileCheckBoxes = static_cast<int32_t>(m_fileSelectionCheckBoxes.size());
    const int32_t numberOfFiberOrientFiles = brain->getNumberOfConnectivityFiberOrientationFiles();
    CaretAssert(numberOfFiberOrientFiles <= numberOfFileCheckBoxes);
    for (int32_t iff = 0; iff < numberOfFiberOrientFiles; iff++) {
        CiftiFiberOrientationAdapter* foca = brain->getConnectivityFiberOrientationFile(iff)->getFiberOrientationAdapter();
        if (foca != NULL) {
            foca->setDisplayed(displayGroup,
                               browserTabIndex,
                               m_fileSelectionCheckBoxes[iff]->isChecked());
        }
    }
    
    updateOtherViewControllers();
    EventManager::get()->sendEvent(EventGraphicsUpdateAllWindows().getPointer());
}

/**
 * @return Create and return the spherical samples widget.
 */
QWidget*
FiberOrientationSelectionViewController::createSamplesWidget()
{
    m_displaySphereOrientationsCheckBox = new QCheckBox("Show Orientations");
    QObject::connect(m_displaySphereOrientationsCheckBox, SIGNAL(clicked(bool)),
                     this, SLOT(processAttributesChanges()));
    
    
    m_samplesOpenGLWidget = new FiberSamplesOpenGLWidget(m_displaySphereOrientationsCheckBox);
    m_samplesOpenGLWidget->setMinimumSize(200, 200);
//    m_samplesOpenGLWidget->resize(200, 200);
    
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);
    WuQtUtilities::setLayoutMargins(layout, 2, 2);
    layout->addWidget(m_displaySphereOrientationsCheckBox);
    layout->addWidget(m_samplesOpenGLWidget,
                      100); // stretch factor
    layout->addStretch();
    
    return widget;
}

/**
 * Update other fiber orientation view controllers.
 */
void
FiberOrientationSelectionViewController::updateOtherViewControllers()
{
    for (std::set<FiberOrientationSelectionViewController*>::iterator iter = s_allViewControllers.begin();
         iter != s_allViewControllers.end();
         iter++) {
        FiberOrientationSelectionViewController* fosvc = *iter;
        if (fosvc != this) {
            fosvc->updateViewController();
        }
    }
}

/**
 * Receive events from the event manager.
 *
 * @param event
 *   Event sent by event manager.
 */
void
FiberOrientationSelectionViewController::receiveEvent(Event* event)
{
    bool doUpdate = false;
    
    if (event->getEventType() == EventTypeEnum::EVENT_USER_INTERFACE_UPDATE) {
        EventUserInterfaceUpdate* uiEvent = dynamic_cast<EventUserInterfaceUpdate*>(event);
        CaretAssert(uiEvent);
        
        if (uiEvent->isUpdateForWindow(m_browserWindowIndex)) {
            if (uiEvent->isToolBoxUpdate()) {
                doUpdate = true;
                uiEvent->setEventProcessed();
            }
        }
    }
    
    if (doUpdate) {
        updateViewController();
    }
}


