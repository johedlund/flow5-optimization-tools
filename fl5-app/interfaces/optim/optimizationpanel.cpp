/****************************************************************************

    flow5 application
    Copyright © 2026 Johan Hedlund

    This file is part of flow5.

    flow5 is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    flow5 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flow5.
    If not, see <https://www.gnu.org/licenses/>.


*****************************************************************************/

#include "optimizationpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>

#include <api/foil.h>
#include <api/polar.h>
#include <api/xfoiltask.h>
#include <interfaces/optim/psotaskfoil.h>
#include <interfaces/optim/psotask.h>
#include <interfaces/graphs/containers/graphwt.h>
#include <interfaces/graphs/graph/graph.h>
#include <interfaces/graphs/graph/curve.h>
#include <interfaces/graphs/graph/curvemodel.h>
#include <interfaces/editors/foiledit/foilwt.h>

OptimizationPanel::OptimizationPanel(QWidget *pParent)
    : QWidget(pParent)
{
    setupUI();
    connect(&m_TaskWatcher, &QFutureWatcher<void>::finished, this, &OptimizationPanel::onTaskFinished);
}

OptimizationPanel::~OptimizationPanel()
{
    // Ensure we restore global state if panel is closed during run
    if(m_OldIterLimit != XFoilTask::maxIterations())
        XFoilTask::setMaxIterations(m_OldIterLimit);

    if(m_pGraph) delete m_pGraph;
    if(m_pCurveModel) delete m_pCurveModel;
    clearPreviewFoils();
}

void OptimizationPanel::initPanel(Foil *pFoil, Polar *pPolar)
{
    m_pFoil = pFoil;
    m_pPolar = pPolar;

    if (m_pFoil)
    {
        log("Target Foil: " + QString::fromStdString(m_pFoil->name()));

        // Init constraints with foil values
        m_sbMinThickness->setValue(m_pFoil->maxThickness());
        m_sbMaxThickness->setValue(m_pFoil->maxThickness());
        m_sbMinLERadius->setValue(m_pFoil->LERadius());
        m_sbMinTEGap->setValue(m_pFoil->TEGap());
        m_sbMaxWiggliness->setValue(m_pFoil->wiggliness());
        
        m_sbMinCamber->setValue(m_pFoil->maxCamber());
        m_sbMaxCamber->setValue(m_pFoil->maxCamber());
        m_sbMinXCamber->setValue(m_pFoil->xCamber());
        m_sbMaxXCamber->setValue(m_pFoil->xCamber());
        m_sbMinXThickness->setValue(m_pFoil->xThickness());
        m_sbMaxXThickness->setValue(m_pFoil->xThickness());
        
        // Approx area
        double area = 0.0;
        const int n = m_pFoil->nBaseNodes();
        for(int i=0; i<n-1; ++i) {
            area += (m_pFoil->xb(i) * m_pFoil->yb(i+1) - m_pFoil->xb(i+1) * m_pFoil->yb(i));
        }
        area = std::fabs(0.5 * area);
        m_sbMinArea->setValue(area);

        double t = m_pFoil->maxThickness();
        m_sbMinModulus->setValue(0.12 * t * t);
        
        if (m_pPolar) {
            m_sbReynolds->setValue(m_pPolar->Reynolds() / 1.0e6); // Display in Millions
            m_sbMach->setValue(m_pPolar->Mach());
            m_sbNCrit->setValue(m_pPolar->NCrit());
        }
        
        rebuildSectionPreview();
    }
}

void OptimizationPanel::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);

    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(mainSplitter);

    // --- Left Pane (Visualization: Vertical Split) ---
    auto *visSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    
    // 1. Progress Graph
    m_pGraph = new Graph;
    m_pCurveModel = new CurveModel;
    m_pGraph->setCurveModel(m_pCurveModel);
    m_pGraph->setName("Optimization Progress");
    m_pGraph->setXVariableList({"Iteration"});
    m_pGraph->setYVariableList({"Fitness"});
    m_pGraph->setVariables(0, 0);
    m_pGraph->showXMajGrid(true);
    m_pGraph->showYMajGrid(0, true);
    m_pGraph->setLegendVisible(true);

    m_pFitnessCurve = m_pGraph->addCurve("Best Fitness", AXIS::LEFTYAXIS, true);
    if(m_pFitnessCurve) {
        m_pFitnessCurve->setColor(Qt::blue);
        m_pFitnessCurve->setStipple(Line::SOLID);
    }

    m_pGraphWt = new GraphWt(this);
    m_pGraphWt->setGraph(m_pGraph);
    visSplitter->addWidget(m_pGraphWt);

    // 2. Section Preview
    m_pSectionView = new FoilWt(this);
    m_pSectionView->showLegend(false);
    visSplitter->addWidget(m_pSectionView);

    // 3. Log
    m_LogOutput = new QTextEdit(this);
    m_LogOutput->setReadOnly(true);
    visSplitter->addWidget(m_LogOutput);

    // Set ratios (Graph:Preview:Log -> 2:2:1)
    visSplitter->setStretchFactor(0, 2);
    visSplitter->setStretchFactor(1, 2);
    visSplitter->setStretchFactor(2, 1);

    mainSplitter->addWidget(visSplitter);

    // --- Right Pane (Inspector) ---
    auto *rightScroll = new QScrollArea(mainSplitter);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    
    auto *inspectorWidget = new QWidget;
    auto *inspectorLayout = new QVBoxLayout(inspectorWidget);
    inspectorLayout->setSpacing(10);

    // Header Status
    m_StatusLabel = new QLabel("Ready.", this);
    m_StatusLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    inspectorLayout->addWidget(m_StatusLabel);

    // Config Group
    auto *configGroup = new QGroupBox("Configuration", inspectorWidget);
    auto *configLayout = new QFormLayout(configGroup);
    m_PresetCombo = new QComboBox(this);
    m_PresetCombo->addItem("V1: Y-only base nodes", static_cast<int>(PSOTaskFoil::PresetType::V1_Y_Only));
    m_PresetCombo->addItem("V2: Camber/Thickness", static_cast<int>(PSOTaskFoil::PresetType::V2_Camber_Thickness));
    configLayout->addRow("Preset:", m_PresetCombo);
    inspectorLayout->addWidget(configGroup);

    // Geometry Group (New)
    auto *geoGroup = new QGroupBox("Geometry", inspectorWidget);
    auto *geoLayout = new QFormLayout(geoGroup);
    m_sbOptimPoints = new QSpinBox(this);
    m_sbOptimPoints->setRange(6, 60);
    m_sbOptimPoints->setValue(8);
    m_sbOptimPoints->setSuffix(" pts");
    m_sbOptimPoints->setToolTip("Control points for optimization (fewer = smoother)");
    geoLayout->addRow("Points:", m_sbOptimPoints);

    m_sbBoundsScale = new QDoubleSpinBox(this);
    m_sbBoundsScale->setRange(0.1, 5.0);
    m_sbBoundsScale->setValue(1.0);
    m_sbBoundsScale->setSingleStep(0.1);
    m_sbBoundsScale->setSuffix("x");
    geoLayout->addRow("Bounds Scale:", m_sbBoundsScale);
    inspectorLayout->addWidget(geoGroup);

    // Run Parameters Group (New)
    auto *runGroup = new QGroupBox("Run Parameters", inspectorWidget);
    auto *runLayout = new QFormLayout(runGroup);
    
    m_sbMaxIter = new QSpinBox(this);
    m_sbMaxIter->setRange(1, 1000);
    m_sbMaxIter->setValue(PSOTask::s_MaxIter);
    m_sbMaxIter->setSuffix(" iters");
    runLayout->addRow("Max Iterations:", m_sbMaxIter);

    m_sbReynolds = new QDoubleSpinBox(this);
    m_sbReynolds->setRange(0.01, 100.0);
    m_sbReynolds->setValue(1.0);
    m_sbReynolds->setSuffix(" M");
    m_sbReynolds->setDecimals(2);
    m_sbReynolds->setSingleStep(0.1);
    runLayout->addRow("Reynolds:", m_sbReynolds);

    m_sbMach = new QDoubleSpinBox(this);
    m_sbMach->setRange(0.0, 1.0);
    m_sbMach->setValue(0.0);
    m_sbMach->setSingleStep(0.05);
    runLayout->addRow("Mach:", m_sbMach);

    m_sbNCrit = new QDoubleSpinBox(this);
    m_sbNCrit->setRange(0.0, 20.0);  // NCrit=0 forces transition at LE
    m_sbNCrit->setValue(9.0);
    m_sbNCrit->setSingleStep(0.5);
    runLayout->addRow("NCrit:", m_sbNCrit);
    
    inspectorLayout->addWidget(runGroup);

    // Objectives Group
    auto *objGroup = new QGroupBox("Objectives", inspectorWidget);
    auto *objLayout = new QGridLayout(objGroup);
    objLayout->addWidget(new QLabel("Objective"), 0, 0);
    objLayout->addWidget(new QLabel("Target"), 0, 1);
    
    m_ObjectiveCombo = new QComboBox(this);
    m_ObjectiveCombo->addItem("Minimize Cd", static_cast<int>(PSOTaskFoil::ObjectiveType::MinimizeCd));
    m_ObjectiveCombo->addItem("Maximize L/D", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizeLD));
    m_ObjectiveCombo->addItem("Maximize Cl", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizeCl));
    m_ObjectiveCombo->addItem("Minimize Cm", static_cast<int>(PSOTaskFoil::ObjectiveType::MinimizeCm));
    m_ObjectiveCombo->addItem("Target Cl", static_cast<int>(PSOTaskFoil::ObjectiveType::TargetCl));
    m_ObjectiveCombo->addItem("Target Cm", static_cast<int>(PSOTaskFoil::ObjectiveType::TargetCm));
    m_ObjectiveCombo->addItem("Max Power Factor", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizePowerFactor));
    m_ObjectiveCombo->addItem("Max Endurance Factor", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizeEnduranceFactor));
    
    m_TargetModeCombo = new QComboBox(this);
    m_TargetModeCombo->addItem("Fixed Alpha", static_cast<int>(PSOTaskFoil::TargetMode::Alpha));
    m_TargetModeCombo->addItem("Fixed CL", static_cast<int>(PSOTaskFoil::TargetMode::Cl));

    m_TargetValueSpin = new QDoubleSpinBox(this);
    m_TargetValueSpin->setRange(-20, 20); 
    m_TargetValueSpin->setValue(0.5);

    objLayout->addWidget(m_ObjectiveCombo, 1, 0);
    objLayout->addWidget(m_TargetModeCombo, 1, 1);
    objLayout->addWidget(m_TargetValueSpin, 1, 2);
    inspectorLayout->addWidget(objGroup);

    // Constraints Group
    auto *constraintsGroup = new QGroupBox("Constraints", inspectorWidget);
    auto *conLayout = new QGridLayout(constraintsGroup);
    
    conLayout->addWidget(new QLabel("Constraint"), 0, 0);
    conLayout->addWidget(new QLabel("Enabled"), 0, 1);
    conLayout->addWidget(new QLabel("Limit"), 0, 2);

    int row = 1;
    auto addConRow = [&](QString name, QCheckBox*& chk, QDoubleSpinBox*& sb, double val, double step, double max) {
        chk = new QCheckBox(this); 
        sb = new QDoubleSpinBox(this);
        sb->setRange(0, max);
        sb->setValue(val);
        sb->setSingleStep(step);
        sb->setDecimals(4);
        sb->setEnabled(false);
        connect(chk, &QCheckBox::toggled, sb, &QDoubleSpinBox::setEnabled);
        
        conLayout->addWidget(new QLabel(name), row, 0);
        conLayout->addWidget(chk, row, 1);
        conLayout->addWidget(sb, row, 2);
        row++;
    };

    addConRow("Min Thickness (t/c)", m_chkMinThickness, m_sbMinThickness, 0.08, 0.001, 0.5);
    addConRow("Max Thickness (t/c)", m_chkMaxThickness, m_sbMaxThickness, 0.15, 0.001, 0.5);
    addConRow("Min LE Radius", m_chkMinLERadius, m_sbMinLERadius, 0.01, 0.001, 0.1);
    addConRow("Min TE Thickness", m_chkMinTEGap, m_sbMinTEGap, 0.002, 0.0005, 0.05);
    addConRow("Max Wiggliness", m_chkMaxWiggliness, m_sbMaxWiggliness, 1.0, 0.1, 100.0);
    addConRow("Min Section Modulus", m_chkMinModulus, m_sbMinModulus, 0.001, 0.0001, 0.1);

    addConRow("Min Camber", m_chkMinCamber, m_sbMinCamber, 0.0, 0.001, 0.2);
    addConRow("Max Camber", m_chkMaxCamber, m_sbMaxCamber, 0.1, 0.001, 0.2);
    addConRow("Min X Camber", m_chkMinXCamber, m_sbMinXCamber, 0.1, 0.05, 0.9);
    addConRow("Max X Camber", m_chkMaxXCamber, m_sbMaxXCamber, 0.5, 0.05, 0.9);
    addConRow("Min X Thickness", m_chkMinXThickness, m_sbMinXThickness, 0.1, 0.05, 0.9);
    addConRow("Max X Thickness", m_chkMaxXThickness, m_sbMaxXThickness, 0.5, 0.05, 0.9);
    addConRow("Min Area", m_chkMinArea, m_sbMinArea, 0.01, 0.001, 0.5);

    addConRow("Min Cl", m_chkMinCl, m_sbMinCl, 0.0, 0.1, 5.0);
    addConRow("Max Cl", m_chkMaxCl, m_sbMaxCl, 1.5, 0.1, 5.0);
    addConRow("Min Cd", m_chkMinCd, m_sbMinCd, 0.0, 0.0001, 0.1);
    addConRow("Max Cd", m_chkMaxCd, m_sbMaxCd, 0.02, 0.001, 0.5);
    addConRow("Min Cm", m_chkMinCm, m_sbMinCm, -0.5, 0.01, 0.5);
    addConRow("Max Cm", m_chkMaxCm, m_sbMaxCm, 0.0, 0.01, 0.5);
    addConRow("Min L/D", m_chkMinLD, m_sbMinLD, 10.0, 1.0, 200.0);

    inspectorLayout->addWidget(constraintsGroup);

    inspectorLayout->addStretch();
    
    // Actions Group (Footer)
    auto *actionGroup = new QGroupBox("Actions", inspectorWidget);
    auto *actionLayout = new QVBoxLayout(actionGroup);
    
    m_ProgressBar = new QProgressBar(this);
    m_ProgressBar->setRange(0, 100);
    m_ProgressBar->setValue(0);
    actionLayout->addWidget(m_ProgressBar);

    auto *btnLayout = new QHBoxLayout;
    m_RunButton = new QPushButton("Run", this);
    connect(m_RunButton, &QPushButton::clicked, this, &OptimizationPanel::onRun);
    btnLayout->addWidget(m_RunButton);

    m_CancelButton = new QPushButton("Stop", this);
    connect(m_CancelButton, &QPushButton::clicked, this, &OptimizationPanel::onCancel);
    m_CancelButton->setEnabled(false);
    btnLayout->addWidget(m_CancelButton);

    m_ApplyBestButton = new QPushButton("Apply", this);
    connect(m_ApplyBestButton, &QPushButton::clicked, this, &OptimizationPanel::onApplyBest);
    m_ApplyBestButton->setEnabled(false);
    btnLayout->addWidget(m_ApplyBestButton);

    m_CloseButton = new QPushButton("Close", this);
    connect(m_CloseButton, &QPushButton::clicked, this, &OptimizationPanel::closeRequested);
    btnLayout->addWidget(m_CloseButton);

    actionLayout->addLayout(btnLayout);
    inspectorLayout->addWidget(actionGroup);

    rightScroll->setWidget(inspectorWidget);
    mainSplitter->addWidget(rightScroll);

    // Initial sizes
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 2);
}

void OptimizationPanel::log(const QString &msg)
{
    m_LogOutput->append(msg);
}

void OptimizationPanel::onRun()
{
    if(!m_pFoil) {
        QMessageBox::warning(this, "Error", "No foil selected.");
        return;
    }

    if(m_TaskWatcher.isRunning()) return;

    if(m_pTask) {
        delete m_pTask;
        m_pTask = nullptr;
    }

    m_pTask = new PSOTaskFoil();
    m_pTask->setParent(this);

    m_pTask->setFoil(m_pFoil);
    m_pTask->setPolar(m_pPolar);

    // Set Run Parameters
    m_pTask->setReynolds(m_sbReynolds->value() * 1.0e6);
    m_pTask->setMach(m_sbMach->value());
    m_pTask->setNCrit(m_sbNCrit->value());

    PSOTask::s_MaxIter = m_sbMaxIter->value();

    PSOTaskFoil::PresetType preset = static_cast<PSOTaskFoil::PresetType>(m_PresetCombo->currentData().toInt());
    m_pTask->setPreset(preset);

    m_pTask->setOptimizationPoints(m_sbOptimPoints->value());
    m_pTask->setBoundsScale(m_sbBoundsScale->value());

    m_pTask->initVariablesFromFoil();

    if(m_pTask->nVariables() == 0) {
        delete m_pTask;
        m_pTask = nullptr;
        QMessageBox::warning(this, "Error", "Foil has no optimizable variables.");
        return;
    }

    // Validate Constraints
    auto checkMinMax = [&](QCheckBox* minChk, QDoubleSpinBox* minSb, 
                           QCheckBox* maxChk, QDoubleSpinBox* maxSb, const QString& name) -> bool {
        if(minChk->isChecked() && maxChk->isChecked() && minSb->value() > maxSb->value()) {
            QMessageBox::warning(this, "Input Error", QString("Min %1 cannot be greater than Max %1.").arg(name));
            return false;
        }
        return true;
    };

    if(!checkMinMax(m_chkMinThickness, m_sbMinThickness, m_chkMaxThickness, m_sbMaxThickness, "Thickness")) return;
    if(!checkMinMax(m_chkMinCamber, m_sbMinCamber, m_chkMaxCamber, m_sbMaxCamber, "Camber")) return;
    if(!checkMinMax(m_chkMinXCamber, m_sbMinXCamber, m_chkMaxXCamber, m_sbMaxXCamber, "XCamber")) return;
    if(!checkMinMax(m_chkMinXThickness, m_sbMinXThickness, m_chkMaxXThickness, m_sbMaxXThickness, "XThickness")) return;
    if(!checkMinMax(m_chkMinCl, m_sbMinCl, m_chkMaxCl, m_sbMaxCl, "Cl")) return;
    if(!checkMinMax(m_chkMinCd, m_sbMinCd, m_chkMaxCd, m_sbMaxCd, "Cd")) return;
    if(!checkMinMax(m_chkMinCm, m_sbMinCm, m_chkMaxCm, m_sbMaxCm, "Cm")) return;

    // Configure Objectives
    m_pTask->setNObjectives(1);
    m_pTask->setObjective(0, OptObjective("Fitness", 0, true, 0.0, 0.0, xfl::MINIMIZE));

    PSOTaskFoil::ObjectiveType objType = static_cast<PSOTaskFoil::ObjectiveType>(m_ObjectiveCombo->currentData().toInt());
    m_pTask->setObjectiveType(objType);

    m_BestValid = false;

    // Configure Target
    PSOTaskFoil::TargetMode targetMode = static_cast<PSOTaskFoil::TargetMode>(m_TargetModeCombo->currentData().toInt());
    double targetVal = m_TargetValueSpin->value();
    
    if (targetMode == PSOTaskFoil::TargetMode::Alpha)
        m_pTask->setTargetAlpha(targetVal);
    else
        m_pTask->setTargetCl(targetVal);

    // Configure Constraints
    PSOTaskFoil::Constraints constraints;
    constraints.enabled = true;
    constraints.minThickness.enabled = m_chkMinThickness->isChecked();
    constraints.minThickness.value = m_sbMinThickness->value();
    constraints.maxThickness.enabled = m_chkMaxThickness->isChecked();
    constraints.maxThickness.value = m_sbMaxThickness->value();
    constraints.minLERadius.enabled = m_chkMinLERadius->isChecked();
    constraints.minLERadius.value = m_sbMinLERadius->value();
    constraints.minTEThickness.enabled = m_chkMinTEGap->isChecked();
    constraints.minTEThickness.value = m_sbMinTEGap->value();
    constraints.maxWiggliness.enabled = m_chkMaxWiggliness->isChecked();
    constraints.maxWiggliness.value = m_sbMaxWiggliness->value();
    constraints.minSectionModulus.enabled = m_chkMinModulus->isChecked();
    constraints.minSectionModulus.value = m_sbMinModulus->value();

    constraints.minCamber.enabled = m_chkMinCamber->isChecked();
    constraints.minCamber.value = m_sbMinCamber->value();
    constraints.maxCamber.enabled = m_chkMaxCamber->isChecked();
    constraints.maxCamber.value = m_sbMaxCamber->value();

    constraints.minXCamber.enabled = m_chkMinXCamber->isChecked();
    constraints.minXCamber.value = m_sbMinXCamber->value();
    constraints.maxXCamber.enabled = m_chkMaxXCamber->isChecked();
    constraints.maxXCamber.value = m_sbMaxXCamber->value();

    constraints.minXThickness.enabled = m_chkMinXThickness->isChecked();
    constraints.minXThickness.value = m_sbMinXThickness->value();
    constraints.maxXThickness.enabled = m_chkMaxXThickness->isChecked();
    constraints.maxXThickness.value = m_sbMaxXThickness->value();

    constraints.minArea.enabled = m_chkMinArea->isChecked();
    constraints.minArea.value = m_sbMinArea->value();

    constraints.minCl.enabled = m_chkMinCl->isChecked();
    constraints.minCl.value = m_sbMinCl->value();
    constraints.maxCl.enabled = m_chkMaxCl->isChecked();
    constraints.maxCl.value = m_sbMaxCl->value();

    constraints.minCd.enabled = m_chkMinCd->isChecked();
    constraints.minCd.value = m_sbMinCd->value();
    constraints.maxCd.enabled = m_chkMaxCd->isChecked();
    constraints.maxCd.value = m_sbMaxCd->value();

    constraints.minCm.enabled = m_chkMinCm->isChecked();
    constraints.minCm.value = m_sbMinCm->value();
    constraints.maxCm.enabled = m_chkMaxCm->isChecked();
    constraints.maxCm.value = m_sbMaxCm->value();

    constraints.minLD.enabled = m_chkMinLD->isChecked();
    constraints.minLD.value = m_sbMinLD->value();

    m_pTask->setConstraints(constraints);

    updateUI(true);
    m_StatusLabel->setText("Optimizing...");
    m_ProgressBar->setRange(0, 0);
    m_ProgressBar->setValue(0);
    m_LogOutput->clear();
    log("Optimization started.");

    m_pFitnessCurve->clear();
    m_pGraph->resetLimits();
    m_pGraphWt->update();
    rebuildSectionPreview();

    m_RunActive = true;
    QCoreApplication::processEvents();

    // Reduce XFoil max iterations for optimization to avoid long hangs on bad particles
    m_OldIterLimit = XFoilTask::maxIterations();
    XFoilTask::setMaxIterations(30);

    m_pTask->onMakeParticleSwarm();
    m_pTask->onStartIterations();
    
    onTaskFinished();
}

void OptimizationPanel::onCancel()
{
    if(m_pTask) {
        m_pTask->cancelAnalyis();
        m_StatusLabel->setText("Cancelling...");
        log("Cancel requested...");
    }
}

void OptimizationPanel::onApplyBest()
{
    if(!m_pTask || !m_BestValid) return;

    Foil *pNewFoil = m_pTask->createOptimizedFoil(m_BestParticle);
    if(pNewFoil)
    {
        emit foilCreated(pNewFoil);
        m_StatusLabel->setText("Foil created: " + QString::fromStdString(pNewFoil->name()));
        log("Foil created: " + QString::fromStdString(pNewFoil->name()));
    }
    else
    {
        QMessageBox::critical(this, "Error", "Failed to create optimized foil geometry.\nThe particle parameters may have resulted in an invalid shape.");
        log("Error: Failed to create optimized foil geometry.");
    }
}

void OptimizationPanel::onTaskFinished()
{
    m_RunActive = false;
    XFoilTask::setMaxIterations(m_OldIterLimit);

    if(m_pTask && m_pTask->isFinished()) return;
    updateUI(false);
}

void OptimizationPanel::updateUI(bool isRunning)
{
    m_RunButton->setEnabled(!isRunning);
    m_CancelButton->setEnabled(isRunning);
    m_ApplyBestButton->setEnabled(!isRunning && m_BestValid);
    m_PresetCombo->setEnabled(!isRunning);
    m_ObjectiveCombo->setEnabled(!isRunning);
}

void OptimizationPanel::customEvent(QEvent *event)
{
    if(event->type() == OPTIM_SWARM_PROGRESS_EVENT)
    {
        OptimEvent *pEvent = static_cast<OptimEvent*>(event);
        if(pEvent) {
             m_ProgressBar->setRange(0, pEvent->iBest()); 
             m_ProgressBar->setValue(pEvent->iter()); 
             m_StatusLabel->setText(QString("Building Swarm %1/%2").arg(pEvent->iter()).arg(pEvent->iBest()));
        }
    }
    else if(event->type() == OPTIM_MAKESWARM_EVENT)
    {
        log("Swarm initialized.");
        m_ProgressBar->setRange(0, PSOTask::s_MaxIter);
        m_ProgressBar->setValue(0);
    }
    else if(event->type() == OPTIM_ITER_EVENT)
    {
        OptimEvent *pEvent = static_cast<OptimEvent*>(event);
        if(pEvent) {
            m_ProgressBar->setValue(pEvent->iter());
            m_StatusLabel->setText(QString("Iteration %1 / %2").arg(pEvent->iter()).arg(PSOTask::s_MaxIter));
            
            double bestFitness = pEvent->particle().fitness(0);
            m_pFitnessCurve->appendPoint(pEvent->iter(), bestFitness);
            if (pEvent->iter() % 5 == 0) {
                m_pGraph->resetLimits();
                m_pGraphWt->update();
                updateCandidatePreview(pEvent->particle());
            }
        }
    }
    else if(event->type() == OPTIM_END_EVENT)
    {
        m_ProgressBar->setValue(PSOTask::s_MaxIter);

        OptimEvent *pEvent = static_cast<OptimEvent*>(event);
        if(pEvent && m_pTask) {
             m_BestParticle = pEvent->particle();
             m_BestValid = m_BestParticle.isConverged();
             log(QString("Best Fitness: %1").arg(m_BestParticle.fitness(0)));
             updateCandidatePreview(m_BestParticle);

             if(m_BestValid) {
                 m_StatusLabel->setText("Optimization finished.");
                 log("Optimization finished. Apply to use the best result.");
                 
                 QString summary = QString("Optimization completed successfully.\n\n"
                                           "Iterations: %1\n"
                                           "Best Fitness: %2\n\n"
                                           "Click 'Apply' to create the optimized foil.")
                                           .arg(PSOTask::s_MaxIter)
                                           .arg(m_BestParticle.fitness(0), 0, 'g', 6);
                 QMessageBox::information(this, "Optimization Results", summary);

             } else {
                 m_StatusLabel->setText("Optimization finished (no valid result).");
                 log("Warning: No converged solution found. All particles may have hit constraints or XFoil failed to converge.");
             }
        } else {
            m_BestValid = false;
            m_StatusLabel->setText("Optimization ended (no result).");
            log("Error: Optimization ended without a valid result.");
        }
        updateUI(false);
    }
}

void OptimizationPanel::clearPreviewFoils()
{
    if(m_pSectionView)
    {
        m_pSectionView->clearFoils();
        m_pSectionView->setBufferFoil(nullptr);
    }

    delete m_pGhostFoil;
    m_pGhostFoil = nullptr;
    delete m_pPreviewFoil;
    m_pPreviewFoil = nullptr;
}

void OptimizationPanel::rebuildSectionPreview()
{
    clearPreviewFoils();

    if(!m_pFoil || !m_pSectionView)
        return;

    m_pGhostFoil = new Foil(m_pFoil);
    m_pGhostFoil->setName(m_pFoil->name() + "_ghost");
    m_pGhostFoil->setLineStipple(Line::DASH);
    m_pGhostFoil->setLineWidth(std::max(1, m_pFoil->lineWidth()));
    m_pGhostFoil->setLineColor(fl5Color(160, 160, 160));
    m_pGhostFoil->setFilled(false);
    m_pGhostFoil->setVisible(true);

    m_pPreviewFoil = new Foil(m_pFoil);
    m_pPreviewFoil->setName(m_pFoil->name() + "_candidate");
    m_pPreviewFoil->setLineStipple(Line::SOLID);
    m_pPreviewFoil->setLineWidth(std::max(2, m_pFoil->lineWidth()));
    m_pPreviewFoil->setLineColor(m_pFoil->lineColor());
    m_pPreviewFoil->setFilled(false);
    m_pPreviewFoil->setVisible(true);

    m_pSectionView->addFoil(m_pGhostFoil);
    m_pSectionView->setBufferFoil(m_pPreviewFoil);
    m_pSectionView->update();
}

void OptimizationPanel::updateCandidatePreview(const Particle &particle)
{
    if(!m_pTask || !m_pSectionView)
        return;

    Foil *pNewFoil = m_pTask->createOptimizedFoil(particle);
    if(!pNewFoil)
        return;

    pNewFoil->setLineStipple(Line::SOLID);
    if(m_pFoil)
    {
        pNewFoil->setLineWidth(std::max(2, m_pFoil->lineWidth()));
        pNewFoil->setLineColor(m_pFoil->lineColor());
    }
    pNewFoil->setFilled(false);
    pNewFoil->setVisible(true);

    delete m_pPreviewFoil;
    m_pPreviewFoil = pNewFoil;

    m_pSectionView->setBufferFoil(m_pPreviewFoil);
    m_pSectionView->update();
}
