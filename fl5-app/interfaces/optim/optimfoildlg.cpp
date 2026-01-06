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

#include <interfaces/optim/optimfoildlg.h>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QMessageBox>
#include <QComboBox>
#include <QtConcurrent/QtConcurrent>

#include <api/foil.h> // Required for m_pFoil->name()
#include <api/polar.h>
#include <interfaces/editors/foiledit/foilwt.h>
#include <interfaces/optim/psotaskfoil.h>
#include <interfaces/optim/psotask.h> // for OptimEvent and constants
#include <interfaces/graphs/containers/graphwt.h>
#include <interfaces/graphs/graph/graph.h>
#include <interfaces/graphs/graph/curve.h>
#include <interfaces/graphs/graph/curvemodel.h>

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>

#include <algorithm>

OptimFoilDlg::OptimFoilDlg(QWidget *pParent)
    : QDialog(pParent)
{
    setWindowTitle("Optimization 2d (Foil)");
    resize(600, 500);

    connect(&m_TaskWatcher, &QFutureWatcher<void>::finished, this, &OptimFoilDlg::onTaskFinished);

    auto *mainLayout = new QVBoxLayout(this);

    m_StatusLabel = new QLabel("Ready to optimize.", this);
    mainLayout->addWidget(m_StatusLabel);

    // Graph setup
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

    // Create curve via Graph API
    m_pFitnessCurve = m_pGraph->addCurve("Best Fitness", AXIS::LEFTYAXIS, true);
    if(m_pFitnessCurve) {
        m_pFitnessCurve->setColor(Qt::blue);
        m_pFitnessCurve->setStipple(Line::SOLID);
    }

    m_pGraphWt = new GraphWt(this);
    m_pGraphWt->setGraph(m_pGraph);
    m_pGraphWt->setMinimumHeight(300);
    mainLayout->addWidget(m_pGraphWt);

    auto *previewLabel = new QLabel("Section preview (best per iteration)", this);
    mainLayout->addWidget(previewLabel);

    m_pSectionView = new FoilWt(this);
    m_pSectionView->showLegend(false);
    m_pSectionView->setMinimumHeight(220);
    mainLayout->addWidget(m_pSectionView);

    auto *presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel("Variables Preset:", this));
    m_PresetCombo = new QComboBox(this);
    m_PresetCombo->addItem("V1: Y-only base nodes", static_cast<int>(PSOTaskFoil::PresetType::V1_Y_Only));
    m_PresetCombo->addItem("V2: Camber/Thickness", static_cast<int>(PSOTaskFoil::PresetType::V2_Camber_Thickness));
    presetLayout->addWidget(m_PresetCombo);
    mainLayout->addLayout(presetLayout);

    auto *objLayout = new QHBoxLayout();
    objLayout->addWidget(new QLabel("Objective:", this));
    m_ObjectiveCombo = new QComboBox(this);
    m_ObjectiveCombo->addItem("Minimize Cd", static_cast<int>(PSOTaskFoil::ObjectiveType::MinimizeCd));
    m_ObjectiveCombo->addItem("Maximize L/D", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizeLD));
    objLayout->addWidget(m_ObjectiveCombo);
    
    objLayout->addWidget(new QLabel("Target:", this));
    m_TargetModeCombo = new QComboBox(this);
    m_TargetModeCombo->addItem("Fixed Alpha", static_cast<int>(PSOTaskFoil::TargetMode::Alpha));
    m_TargetModeCombo->addItem("Fixed CL", static_cast<int>(PSOTaskFoil::TargetMode::Cl));
    objLayout->addWidget(m_TargetModeCombo);

    m_TargetValueSpin = new QDoubleSpinBox(this);
    m_TargetValueSpin->setRange(-20, 20); 
    m_TargetValueSpin->setValue(0.5);
    objLayout->addWidget(m_TargetValueSpin);

    mainLayout->addLayout(objLayout);

    auto *constraintsGroup = new QGroupBox("Constraints", this);
    auto *conLayout = new QFormLayout(constraintsGroup);

    auto addConstraint = [&](QString name, QCheckBox*& chk, QDoubleSpinBox*& sb, double val, double step, double max) {
        auto *hLayout = new QHBoxLayout();
        chk = new QCheckBox(name, this);
        sb = new QDoubleSpinBox(this);
        sb->setRange(0, max);
        sb->setValue(val);
        sb->setSingleStep(step);
        sb->setDecimals(4);
        sb->setEnabled(false);
        connect(chk, &QCheckBox::toggled, sb, &QDoubleSpinBox::setEnabled);
        hLayout->addWidget(chk);
        hLayout->addWidget(sb);
        conLayout->addRow(hLayout);
    };

    addConstraint("Min Thickness (t/c)", m_chkMinThickness, m_sbMinThickness, 0.08, 0.001, 0.5);
    addConstraint("Max Thickness (t/c)", m_chkMaxThickness, m_sbMaxThickness, 0.15, 0.001, 0.5);
    addConstraint("Min LE Radius", m_chkMinLERadius, m_sbMinLERadius, 0.01, 0.001, 0.1);
    addConstraint("Min TE Thickness", m_chkMinTEGap, m_sbMinTEGap, 0.002, 0.0005, 0.05);
    addConstraint("Max Wiggliness", m_chkMaxWiggliness, m_sbMaxWiggliness, 1.0, 0.1, 100.0);
    addConstraint("Min Section Modulus", m_chkMinModulus, m_sbMinModulus, 0.001, 0.0001, 0.1);

    mainLayout->addWidget(constraintsGroup);

    m_ProgressBar = new QProgressBar(this);
    m_ProgressBar->setRange(0, 100);
    m_ProgressBar->setValue(0);
    mainLayout->addWidget(m_ProgressBar);

    auto *buttonLayout = new QHBoxLayout();
    
    m_RunButton = new QPushButton("Run", this);
    connect(m_RunButton, &QPushButton::clicked, this, &OptimFoilDlg::onRun);
    buttonLayout->addWidget(m_RunButton);

    m_CancelButton = new QPushButton("Cancel", this);
    connect(m_CancelButton, &QPushButton::clicked, this, &OptimFoilDlg::onCancel);
    m_CancelButton->setEnabled(false);
    buttonLayout->addWidget(m_CancelButton);

    m_ApplyBestButton = new QPushButton("Apply Best", this);
    connect(m_ApplyBestButton, &QPushButton::clicked, this, &OptimFoilDlg::onApplyBest);
    m_ApplyBestButton->setEnabled(false);
    buttonLayout->addWidget(m_ApplyBestButton);

    auto *closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);
}

OptimFoilDlg::~OptimFoilDlg()
{
    if(m_pGraph) delete m_pGraph;
    if(m_pCurveModel) delete m_pCurveModel;
    clearPreviewFoils();
}

void OptimFoilDlg::initDialog(Foil *pFoil, Polar *pPolar)
{
    m_pFoil = pFoil;
    m_pPolar = pPolar;
    
    if (m_pFoil)
    {
        m_StatusLabel->setText("Target Foil: " + QString::fromStdString(m_pFoil->name()));

        m_sbMinThickness->setValue(m_pFoil->maxThickness());
        m_sbMaxThickness->setValue(m_pFoil->maxThickness());
        m_sbMinLERadius->setValue(m_pFoil->LERadius());
        m_sbMinTEGap->setValue(m_pFoil->TEGap());
        m_sbMaxWiggliness->setValue(m_pFoil->wiggliness());
        
        double t = m_pFoil->maxThickness();
        m_sbMinModulus->setValue(0.12 * t * t);
    }

    rebuildSectionPreview();
}

void OptimFoilDlg::onRun()
{
    if(!m_pFoil || !m_pPolar) {
        QMessageBox::warning(this, "Error", "No foil or polar selected.");
        return;
    }

    if(m_TaskWatcher.isRunning())
    {
        QMessageBox::information(this, "Optimization",
                                 "Optimization is already running.");
        return;
    }

    // Fix race condition: only delete if not running
    if(m_pTask) {
        if(m_TaskWatcher.isRunning()) {
            // Should not reach here due to check above, but be safe
            QMessageBox::warning(this, "Error", "Previous task still running.");
            return;
        }
        delete m_pTask;
        m_pTask = nullptr;
    }

    m_pTask = new PSOTaskFoil();
    m_pTask->setParent(this); // Critical for event reception

    m_pTask->setFoil(m_pFoil);
    m_pTask->setPolar(m_pPolar);

    PSOTaskFoil::PresetType preset = static_cast<PSOTaskFoil::PresetType>(m_PresetCombo->currentData().toInt());
    m_pTask->setPreset(preset);

    m_pTask->initVariablesFromFoil();

    // Validate we have optimizable variables
    if(m_pTask->nVariables() == 0) {
        delete m_pTask;
        m_pTask = nullptr;
        QMessageBox::warning(this, "Error",
                             "Foil has no optimizable variables.\n"
                             "The foil may have too few control points.");
        return;
    }

    // Configure Objectives from UI
    m_pTask->setNObjectives(1);
    // Always minimize (we handle sign in calcFitness)
    m_pTask->setObjective(0, OptObjective("Fitness", 0, true, 0.0, 0.0, xfl::MINIMIZE));

    PSOTaskFoil::ObjectiveType objType = static_cast<PSOTaskFoil::ObjectiveType>(m_ObjectiveCombo->currentData().toInt());
    m_pTask->setObjectiveType(objType);

    // Configure Target from UI
    PSOTaskFoil::TargetMode targetMode = static_cast<PSOTaskFoil::TargetMode>(m_TargetModeCombo->currentData().toInt());
    double targetVal = m_TargetValueSpin->value();
    
    if (targetMode == PSOTaskFoil::TargetMode::Alpha)
        m_pTask->setTargetAlpha(targetVal);
    else
        m_pTask->setTargetCl(targetVal);

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

    m_pTask->setConstraints(constraints);

    rebuildSectionPreview();

    updateUI(true);
    m_StatusLabel->setText("Building swarm...");
    m_ProgressBar->setRange(0, 0);
    m_ProgressBar->setValue(0);

    m_pFitnessCurve->clear();
    m_pGraph->resetLimits();
    m_pGraph->invalidate();
    m_pGraphWt->update();

    m_RunActive = true;

    // Run synchronously in the main thread to avoid OpenBLAS threading issues.
    // The UI is kept alive via QCoreApplication::processEvents() in the task loops.
    QCoreApplication::processEvents(); // Update UI before starting

    m_pTask->onMakeParticleSwarm();
    m_pTask->onStartIterations();
    
    onTaskFinished();
}

void OptimFoilDlg::onCancel()
{
    if(m_pTask) {
        m_pTask->cancelAnalyis(); // note typo in original class 'cancelAnalyis'
        m_StatusLabel->setText("Cancelling...");
    }
}

void OptimFoilDlg::onApplyBest()
{
    if(!m_pTask || !m_BestValid)
        return;

    Foil *pNewFoil = m_pTask->createOptimizedFoil(m_BestParticle);
    if(pNewFoil)
    {
        emit foilCreated(pNewFoil);
        m_StatusLabel->setText("Foil created: " + QString::fromStdString(pNewFoil->name()));
    }
}

void OptimFoilDlg::onTaskFinished()
{
    m_RunActive = false;
    if(m_pTask && m_pTask->isFinished())
        return;
    updateUI(false);
}

void OptimFoilDlg::customEvent(QEvent *event)
{
    if(event->type() == OPTIM_MAKESWARM_EVENT)
    {
        m_StatusLabel->setText("Swarm initialized.");
        m_ProgressBar->setRange(0, PSOTask::s_MaxIter);
        m_ProgressBar->setValue(0);
    }
    else if(event->type() == OPTIM_ITER_EVENT)
    {
        OptimEvent *pEvent = static_cast<OptimEvent*>(event);
        if(pEvent) {
            m_ProgressBar->setValue(pEvent->iter());
            m_StatusLabel->setText(QString("Iteration %1 / %2").arg(pEvent->iter()).arg(PSOTask::s_MaxIter));
            
            // Update graph
            double bestFitness = pEvent->particle().fitness(0); // Assuming objective 0 is primary
            m_pFitnessCurve->appendPoint(pEvent->iter(), bestFitness);
            m_pGraph->resetLimits();
            m_pGraph->invalidate();
            m_pGraphWt->update();

            updateCandidatePreview(pEvent->particle());
        }
    }
    else if(event->type() == OPTIM_END_EVENT)
    {
        m_StatusLabel->setText("Optimization finished.");
        m_ProgressBar->setValue(PSOTask::s_MaxIter);
        m_pGraph->resetLimits();
        m_pGraph->invalidate();
        m_pGraphWt->update();
        
        // Show best result if available
        OptimEvent *pEvent = static_cast<OptimEvent*>(event);
        if(pEvent && m_pTask) {
             m_BestParticle = pEvent->particle();
             m_BestValid = m_BestParticle.isConverged();
             QMessageBox::information(this, "Done", 
                QString("Optimization completed.\nBest Cl: %1")
                .arg(pEvent->particle().fitness(0)));
        }
        updateUI(false);
    }
}

void OptimFoilDlg::clearPreviewFoils()
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

void OptimFoilDlg::rebuildSectionPreview()
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

void OptimFoilDlg::updateCandidatePreview(const Particle &particle)
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

void OptimFoilDlg::updateUI(bool isRunning)
{
    m_RunButton->setEnabled(!isRunning);
    m_CancelButton->setEnabled(isRunning);
    m_ApplyBestButton->setEnabled(!isRunning && m_BestValid);
    m_PresetCombo->setEnabled(!isRunning);
    // Disable inputs if we had any
}
