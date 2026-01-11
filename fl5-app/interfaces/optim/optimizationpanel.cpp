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

#include <vector>
#include <tuple>

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
#include <QRadioButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>
#include <QTimer>

#include <api/foil.h>
#include <api/polar.h>
#include <api/xfoiltask.h>
#include <objects3d.h>
#include <plane.h>
#include <planexfl.h>
#include <wingxfl.h>
#include <wingsection.h>
#include <interfaces/optim/psotaskfoil.h>
#include <interfaces/optim/psotask.h>
#include <interfaces/optim/inducedaoaadapter.h>
#include <interfaces/graphs/controls/graphoptions.h>
#include <interfaces/graphs/containers/graphwt.h>
#include <interfaces/graphs/graph/graph.h>
#include <interfaces/graphs/graph/curve.h>
#include <interfaces/graphs/graph/curvemodel.h>
#include <interfaces/editors/foiledit/foilwt.h>

namespace {

QString objectiveTypeName(PSOTaskFoil::ObjectiveType type)
{
    switch(type) {
        case PSOTaskFoil::ObjectiveType::MinimizeCd:
            return "Min Cd";
        case PSOTaskFoil::ObjectiveType::MaximizeLD:
            return "Max L/D";
        case PSOTaskFoil::ObjectiveType::MaximizeCl:
            return "Max Cl";
        case PSOTaskFoil::ObjectiveType::MinimizeCm:
            return "Min Cm";
        case PSOTaskFoil::ObjectiveType::TargetCl:
            return "Target Cl";
        case PSOTaskFoil::ObjectiveType::TargetCm:
            return "Target Cm";
        case PSOTaskFoil::ObjectiveType::MaximizePowerFactor:
            return "Max Power";
        case PSOTaskFoil::ObjectiveType::MaximizeEnduranceFactor:
            return "Max Endur";
    }
    return "Objective";
}

QString objectiveMetricLabel(PSOTaskFoil::ObjectiveType type)
{
    switch(type) {
        case PSOTaskFoil::ObjectiveType::MinimizeCd:
            return "Cd";
        case PSOTaskFoil::ObjectiveType::MaximizeLD:
            return "L/D";
        case PSOTaskFoil::ObjectiveType::MaximizeCl:
            return "Cl";
        case PSOTaskFoil::ObjectiveType::MinimizeCm:
            return "Abs Cm";
        case PSOTaskFoil::ObjectiveType::TargetCl:
            return "Cl error";
        case PSOTaskFoil::ObjectiveType::TargetCm:
            return "Cm error";
        case PSOTaskFoil::ObjectiveType::MaximizePowerFactor:
            return "Power factor";
        case PSOTaskFoil::ObjectiveType::MaximizeEnduranceFactor:
            return "Endurance factor";
    }
    return "Objective";
}

double objectiveMetricFromFitness(PSOTaskFoil::ObjectiveType type, double fitness)
{
    if(fitness >= OPTIM_PENALTY)
        return fitness;

    switch(type) {
        case PSOTaskFoil::ObjectiveType::MaximizeLD:
        case PSOTaskFoil::ObjectiveType::MaximizeCl:
        case PSOTaskFoil::ObjectiveType::MaximizePowerFactor:
        case PSOTaskFoil::ObjectiveType::MaximizeEnduranceFactor:
            return (fitness < 0.0) ? -fitness : fitness;
        default:
            break;
    }

    return fitness;
}

} // namespace

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

        if (m_pPolar) {
            m_sbReynolds->setValue(m_pPolar->Reynolds() / 1.0e6); // Display in Millions
            m_sbMach->setValue(m_pPolar->Mach());
            m_sbNCrit->setValue(m_pPolar->NCrit());
        }

        rebuildSectionPreview();
        updateOptimMarkersPreview();
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
    m_pGraph->setYVariableList({"Best Fitness", "Objective"});
    m_pGraph->setVariables(0, 0, 1);
    m_pGraph->enableRightAxis(true);
    m_pGraph->showRightAxis(true);
    m_pGraph->setScaleType(GRAPH::EXPANDING);
    GraphOptions::resetGraphSettings(*m_pGraph);
    m_pGraph->showXMajGrid(true);
    m_pGraph->showYMajGrid(0, true);
    m_pGraph->showXMinGrid(true);
    m_pGraph->showYMinGrid(0, true);
    m_pGraph->showYMajGrid(1, false);
    m_pGraph->showYMinGrid(1, false);
    m_pGraph->setLegendVisible(true);
    m_pGraph->setLegendPosition(Qt::AlignTop | Qt::AlignHCenter);

    m_pFitnessCurve = m_pGraph->addCurve("Best Fitness", AXIS::LEFTYAXIS, true);
    if(m_pFitnessCurve) {
        m_pFitnessCurve->setColor(Qt::blue);
        m_pFitnessCurve->setStipple(Line::SOLID);
        m_pFitnessCurve->setWidth(2);
    }

    m_pMetricCurve = m_pGraph->addCurve("Objective", AXIS::RIGHTYAXIS, true);
    if(m_pMetricCurve) {
        m_pMetricCurve->setColor(Qt::darkGreen);
        m_pMetricCurve->setStipple(Line::DASH);
        m_pMetricCurve->setWidth(2);
    }

    m_pGraphWt = new GraphWt(this);
    m_pGraphWt->setGraph(m_pGraph);
    visSplitter->addWidget(m_pGraphWt);

    // 2. Section Preview
    m_pSectionView = new FoilWt(this);
    m_pSectionView->showLegend(false);
    m_pSectionView->showOptimMarkers(true);
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
    m_PresetCombo->addItem("V3: B-spline control", static_cast<int>(PSOTaskFoil::PresetType::V3_BSpline_Control));
    m_PresetCombo->setToolTip("Select the parametrization method for the foil geometry.\n"
                              "V1: Modifies Y-coordinates of base nodes (interpolating).\n"
                              "V2: Optimizes Camber and Thickness distributions directly.\n"
                              "V3: Modifies B-spline control points (approximating, smoother).");
    connect(m_PresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OptimizationPanel::updateOptimMarkersPreview);
    configLayout->addRow("Preset:", m_PresetCombo);
    inspectorLayout->addWidget(configGroup);

    // Geometry Group (New)
    auto *geoGroup = new QGroupBox("Geometry", inspectorWidget);
    auto *geoLayout = new QFormLayout(geoGroup);
    m_sbOptimPoints = new QSpinBox(this);
    m_sbOptimPoints->setRange(6, 60);
    m_sbOptimPoints->setValue(8);
    m_sbOptimPoints->setSuffix(" pts");
    m_sbOptimPoints->setToolTip("Number of control points used to deform the foil.\n"
                                "Fewer points result in smoother shapes but less flexibility.\n"
                                "More points allow complex shapes but may introduce wiggliness.");
    connect(m_sbOptimPoints, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &OptimizationPanel::updateOptimMarkersPreview);
    geoLayout->addRow("Points:", m_sbOptimPoints);

    m_sbBoundsScale = new QDoubleSpinBox(this);
    m_sbBoundsScale->setRange(0.1, 5.0);
    m_sbBoundsScale->setValue(1.0);
    m_sbBoundsScale->setSingleStep(0.1);
    m_sbBoundsScale->setSuffix("x");
    m_sbBoundsScale->setToolTip("Scales the Y search space for each variable.\n"
                                "Values > 1.0 allow larger deformations from the original foil.\n"
                                "Values < 1.0 constrain the optimization to the neighborhood of the original foil.");
    connect(m_sbBoundsScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OptimizationPanel::updateOptimMarkersPreview);
    geoLayout->addRow("Y Bounds Scale:", m_sbBoundsScale);

    m_sbXBoundsScale = new QDoubleSpinBox(this);
    m_sbXBoundsScale->setRange(0.0, 10.0);
    m_sbXBoundsScale->setValue(2.0);
    m_sbXBoundsScale->setSingleStep(0.5);
    m_sbXBoundsScale->setSuffix("x");
    m_sbXBoundsScale->setToolTip("X bounds multiplier relative to Y bounds.\n"
                                 "2.0 = X can move twice as far as Y.\n"
                                 "Higher values allow more X movement for fine-tuning.");
    connect(m_sbXBoundsScale, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OptimizationPanel::updateOptimMarkersPreview);
    geoLayout->addRow("X Bounds Scale:", m_sbXBoundsScale);

    // X movement chord range (V1 preset)
    m_sbXMoveMin = new QDoubleSpinBox(this);
    m_sbXMoveMin->setRange(0.0, 100.0);
    m_sbXMoveMin->setValue(20.0);
    m_sbXMoveMin->setSingleStep(5.0);
    m_sbXMoveMin->setSuffix(" %");
    m_sbXMoveMin->setToolTip("Minimum chord position for X movement.\n"
                             "Control points before this position only move in Y.");
    connect(m_sbXMoveMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OptimizationPanel::updateOptimMarkersPreview);
    geoLayout->addRow("X Move Start:", m_sbXMoveMin);

    m_sbXMoveMax = new QDoubleSpinBox(this);
    m_sbXMoveMax->setRange(0.0, 100.0);
    m_sbXMoveMax->setValue(80.0);
    m_sbXMoveMax->setSingleStep(5.0);
    m_sbXMoveMax->setSuffix(" %");
    m_sbXMoveMax->setToolTip("Maximum chord position for X movement.\n"
                             "Control points after this position only move in Y.");
    connect(m_sbXMoveMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OptimizationPanel::updateOptimMarkersPreview);
    geoLayout->addRow("X Move End:", m_sbXMoveMax);

    m_cbSymmetric = new QCheckBox("Symmetric foil", this);
    m_cbSymmetric->setToolTip("Force the foil to be symmetric (zero camber).\n"
                              "Upper and lower surfaces will be mirrored about the chord line.");
    m_cbSymmetric->setChecked(false);
    connect(m_cbSymmetric, &QCheckBox::toggled,
            this, &OptimizationPanel::updateOptimMarkersPreview);
    geoLayout->addRow("", m_cbSymmetric);
    inspectorLayout->addWidget(geoGroup);

    // Run Parameters Group (New)
    auto *runGroup = new QGroupBox("Run Parameters", inspectorWidget);
    auto *runLayout = new QFormLayout(runGroup);
    
    m_sbMaxIter = new QSpinBox(this);
    m_sbMaxIter->setRange(1, 1000);
    m_sbMaxIter->setValue(PSOTask::s_MaxIter);
    m_sbMaxIter->setSuffix(" iters");
    m_sbMaxIter->setToolTip("Maximum number of iterations for the Particle Swarm Optimizer.\n"
                            "Higher values allow better convergence but take longer.");
    runLayout->addRow("Max Iterations:", m_sbMaxIter);

    m_sbBatchRuns = new QSpinBox(this);
    m_sbBatchRuns->setRange(1, 100);
    m_sbBatchRuns->setValue(1);
    m_sbBatchRuns->setSuffix(" runs");
    m_sbBatchRuns->setToolTip("Number of optimization runs (batch mode).\n"
                              "Each run uses different random seeding.\n"
                              "Best result across all runs is kept.");
    runLayout->addRow("Batch Runs:", m_sbBatchRuns);

    m_sbReynolds = new QDoubleSpinBox(this);
    m_sbReynolds->setRange(0.01, 100.0);
    m_sbReynolds->setValue(1.0);
    m_sbReynolds->setSuffix(" M");
    m_sbReynolds->setDecimals(2);
    m_sbReynolds->setSingleStep(0.1);
    m_sbReynolds->setToolTip("Reynolds number in millions (Re = V*L/nu).\n"
                             "Affects boundary layer transition and drag.");
    runLayout->addRow("Reynolds:", m_sbReynolds);

    m_sbMach = new QDoubleSpinBox(this);
    m_sbMach->setRange(0.0, 1.0);
    m_sbMach->setValue(0.0);
    m_sbMach->setSingleStep(0.05);
    m_sbMach->setToolTip("Mach number (M = V/a).\n"
                         "Compressibility effects are considered if M > 0.3.");
    runLayout->addRow("Mach:", m_sbMach);

    m_sbNCrit = new QDoubleSpinBox(this);
    m_sbNCrit->setRange(0.0, 20.0);  // NCrit=0 forces transition at LE
    m_sbNCrit->setValue(9.0);
    m_sbNCrit->setSingleStep(0.5);
    m_sbNCrit->setToolTip("Critical amplification factor for transition prediction.\n"
                          "9.0: Standard wind tunnel / smooth air.\n"
                          "~0: Fully turbulent flow.");
    runLayout->addRow("NCrit:", m_sbNCrit);

    inspectorLayout->addWidget(runGroup);

    // Mode Selection Group (Mode A vs Mode B)
    auto *modeGroup = new QGroupBox("Optimization Mode", inspectorWidget);
    auto *modeLayout = new QVBoxLayout(modeGroup);

    auto *modeButtonGroup = new QButtonGroup(this);
    m_rbModeA = new QRadioButton("Mode A: 2D only (fixed AoA)", this);
    m_rbModeA->setToolTip("Standard 2D optimization.\n"
                          "Foil is evaluated at the specified geometric angle of attack.\n"
                          "No 3D wing effects are considered.");
    m_rbModeA->setChecked(true);
    m_rbModeB = new QRadioButton("Mode B: 3D coupled (induced AoA)", this);
    m_rbModeB->setToolTip("3D-coupled optimization.\n"
                          "Runs a 3D panel analysis to compute the induced angle of attack\n"
                          "at the selected wing section, then optimizes the foil at the\n"
                          "effective angle (geometric + induced).");
    modeButtonGroup->addButton(m_rbModeA);
    modeButtonGroup->addButton(m_rbModeB);
    modeLayout->addWidget(m_rbModeA);
    modeLayout->addWidget(m_rbModeB);

    // Mode B options (hidden when Mode A selected)
    m_pModeBOptions = new QWidget(this);
    auto *modeBLayout = new QFormLayout(m_pModeBOptions);
    modeBLayout->setContentsMargins(20, 5, 0, 0);

    m_PlaneCombo = new QComboBox(this);
    m_PlaneCombo->setToolTip("Select the plane containing the wing to optimize.");
    modeBLayout->addRow("Plane:", m_PlaneCombo);

    m_WingCombo = new QComboBox(this);
    m_WingCombo->setToolTip("Select the wing (main wing, stab, fin, etc.).");
    modeBLayout->addRow("Wing:", m_WingCombo);

    m_SectionCombo = new QComboBox(this);
    m_SectionCombo->setToolTip("Select the wing section to optimize.\n"
                               "The induced AoA will be computed at this spanwise location.");
    modeBLayout->addRow("Section:", m_SectionCombo);

    m_pModeBOptions->setVisible(false);
    modeLayout->addWidget(m_pModeBOptions);

    connect(m_rbModeA, &QRadioButton::toggled, this, &OptimizationPanel::onModeChanged);
    connect(m_rbModeB, &QRadioButton::toggled, this, &OptimizationPanel::onModeChanged);
    connect(m_PlaneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OptimizationPanel::onPlaneChanged);
    connect(m_WingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OptimizationPanel::onWingChanged);

    inspectorLayout->addWidget(modeGroup);

    // Objectives Group - Dynamic list with Add button (multi-point optimization)
    auto *objGroup = new QGroupBox("Objectives", inspectorWidget);
    auto *objMainLayout = new QVBoxLayout(objGroup);
    objMainLayout->setSpacing(4);

    // Container for objective rows
    m_ObjectiveListLayout = new QVBoxLayout();
    m_ObjectiveListLayout->setSpacing(2);
    objMainLayout->addLayout(m_ObjectiveListLayout);

    // Normalization checkbox
    m_cbNormalizeObjectives = new QCheckBox("Auto-normalize objectives", this);
    m_cbNormalizeObjectives->setToolTip("Automatically normalize objectives based on baseline foil performance.\n"
                                         "This ensures objectives with different scales (e.g., Cd ~0.01, L/D ~50)\n"
                                         "contribute equally to the weighted sum.");
    m_cbNormalizeObjectives->setChecked(true);
    objMainLayout->addWidget(m_cbNormalizeObjectives);

    // Add objective button
    m_AddObjectiveBtn = new QPushButton("+ Add Objective", this);
    m_AddObjectiveBtn->setToolTip("Add a new objective for multi-point optimization");
    connect(m_AddObjectiveBtn, &QPushButton::clicked, this, &OptimizationPanel::addObjectiveRow);
    objMainLayout->addWidget(m_AddObjectiveBtn);

    // Add one default objective row
    addObjectiveRow();

    inspectorLayout->addWidget(objGroup);

    // Constraints Group - Dynamic list with Add button
    auto *constraintsGroup = new QGroupBox("Constraints", inspectorWidget);
    auto *conMainLayout = new QVBoxLayout(constraintsGroup);
    conMainLayout->setSpacing(4);

    // Container for constraint rows
    m_ConstraintListLayout = new QVBoxLayout();
    m_ConstraintListLayout->setSpacing(2);
    conMainLayout->addLayout(m_ConstraintListLayout);

    // Add constraint button
    m_AddConstraintBtn = new QPushButton("+ Add Constraint", this);
    m_AddConstraintBtn->setToolTip("Add a new constraint to the optimization");
    connect(m_AddConstraintBtn, &QPushButton::clicked, this, &OptimizationPanel::addConstraintRow);
    conMainLayout->addWidget(m_AddConstraintBtn);

    // Rejection summary label (hidden until optimization runs)
    m_RejectionSummaryLabel = new QLabel("", this);
    m_RejectionSummaryLabel->setWordWrap(true);
    m_RejectionSummaryLabel->setStyleSheet("color: #888; font-size: 11px;");
    m_RejectionSummaryLabel->hide();
    conMainLayout->addWidget(m_RejectionSummaryLabel);

    // Timer for updating rejection stats during optimization
    m_RejectionUpdateTimer = new QTimer(this);
    m_RejectionUpdateTimer->setInterval(500);  // Update every 500ms
    connect(m_RejectionUpdateTimer, &QTimer::timeout, this, &OptimizationPanel::updateRejectionStats);

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
    m_pTask->setXBoundsScale(m_sbXBoundsScale->value());
    m_pTask->setXMoveChordRange(m_sbXMoveMin->value() / 100.0, m_sbXMoveMax->value() / 100.0);
    m_pTask->setSymmetric(m_cbSymmetric->isChecked());

    m_pTask->initVariablesFromFoil();

    if(m_pTask->nVariables() == 0) {
        delete m_pTask;
        m_pTask = nullptr;
        QMessageBox::warning(this, "Error", "Foil has no optimizable variables.");
        return;
    }

    // Set up optimization markers for visualization (V1 and V3 only)
    {
        std::vector<std::pair<double, double>> ctrlPts;
        std::vector<std::tuple<double, double, double>> bounds;
        m_pTask->getOptimMarkers(ctrlPts, bounds);
        m_pSectionView->setOptimMarkers(ctrlPts, bounds);
        m_pSectionView->update();
    }

    // Configure Objectives (multi-point weighted sum)
    m_pTask->setNObjectives(1);  // Weighted sum produces single fitness value
    m_pTask->setObjective(0, OptObjective("WeightedSum", 0, true, 0.0, 0.0, xfl::MINIMIZE));

    auto objectiveSpecs = buildObjectiveSpecs();
    m_pTask->setObjectiveSpecs(objectiveSpecs);

    // Compute normalization factors from baseline foil
    if(m_cbNormalizeObjectives->isChecked())
    {
        log("Computing normalization factors from base foil...");
        m_pTask->computeNormFactors();
    }

    // Build objective description for graph and logging
    QString objectiveLabel = "Multi-Objective";
    if(objectiveSpecs.size() == 1)
    {
        // Single objective - use its name
        if(!m_ObjectiveRows.isEmpty() && m_ObjectiveRows.first()->objectiveCombo)
            objectiveLabel = m_ObjectiveRows.first()->objectiveCombo->currentText();
    }

    m_pGraph->setName(QString("Optimization Progress - %1").arg(objectiveLabel));

    // Get metric label for right Y axis (use first objective's metric)
    QString metricLabel = "Metric";
    if(!m_ObjectiveRows.isEmpty() && m_ObjectiveRows.first()->objectiveCombo) {
        auto objType = static_cast<PSOTaskFoil::ObjectiveType>(
            m_ObjectiveRows.first()->objectiveCombo->currentData().toInt());
        metricLabel = objectiveMetricLabel(objType);
    }

    m_pGraph->setYVariableList({QString("Fitness (%1)").arg(objectiveLabel), metricLabel});
    m_pGraph->setVariables(0, 0, 1);
    if(m_pFitnessCurve)
        m_pFitnessCurve->setName(QString("Fitness (%1)").arg(objectiveLabel));
    if(m_pMetricCurve)
        m_pMetricCurve->setName(metricLabel);

    m_BestValid = false;

    // Log objectives
    for(const auto &spec : objectiveSpecs)
    {
        QString targetStr = (spec.targetMode == PSOTaskFoil::TargetMode::Alpha)
            ? QString("α=%1°").arg(spec.targetValue, 0, 'f', 1)
            : QString("Cl=%1").arg(spec.targetValue, 0, 'f', 2);
        log(QString("  Objective: %1 @ %2, weight=%3")
            .arg(objectiveTypeName(spec.type))
            .arg(targetStr)
            .arg(spec.weight, 0, 'f', 2));
    }

    // Configure Mode (A or B)
    if (m_rbModeB->isChecked()) {
        // Mode B: 3D coupled optimization
        QString planeName = m_PlaneCombo->currentData().toString();
        PlaneXfl *pPlane = dynamic_cast<PlaneXfl*>(Objects3d::plane(planeName.toStdString()));

        if (!pPlane) {
            QMessageBox::warning(this, "Mode B Error", "No plane selected. Please select a plane for 3D analysis.");
            delete m_pTask;
            m_pTask = nullptr;
            return;
        }

        int wingIndex = m_WingCombo->currentData().toInt();
        int sectionIndex = m_SectionCombo->currentData().toInt();

        // Get target alpha from first objective (if alpha mode) for 3D analysis
        double geometricAlpha = 2.0;  // Default
        if(!objectiveSpecs.empty() && objectiveSpecs.front().targetMode == PSOTaskFoil::TargetMode::Alpha)
            geometricAlpha = objectiveSpecs.front().targetValue;

        // Run 3D analysis to get induced alpha
        log(QString("Mode B: Running 3D analysis for %1, wing %2, section %3...")
            .arg(planeName).arg(wingIndex).arg(sectionIndex));

        InducedAoAAdapter adapter;
        adapter.setPlane(pPlane, wingIndex, sectionIndex);
        adapter.setFlightConditions(geometricAlpha, 30.0, 1.225, 1.5e-5);
        adapter.setNCrit(m_sbNCrit->value());
        adapter.setMach(m_sbMach->value());

        if (!adapter.run()) {
            QString errorMsg = QString("3D analysis failed: %1\n\nFalling back to Mode A.")
                                   .arg(QString::fromStdString(adapter.lastError()));
            QMessageBox::warning(this, "Mode B Warning", errorMsg);
            log("Mode B failed: " + QString::fromStdString(adapter.lastError()));
            log("Continuing with Mode A (no induced AoA correction).");
            m_pTask->setOptimizationMode(PSOTaskFoil::OptimizationMode::ModeA);
            m_CachedInducedAlpha = 0.0;
        } else {
            m_CachedInducedAlpha = adapter.inducedAlpha();
            double effectiveAlpha = adapter.effectiveAlpha();

            log(QString("Mode B: Geometric AoA = %1 deg").arg(geometricAlpha, 0, 'f', 2));
            log(QString("Mode B: Induced AoA = %1 deg").arg(m_CachedInducedAlpha, 0, 'f', 2));
            log(QString("Mode B: Effective AoA = %1 deg").arg(effectiveAlpha, 0, 'f', 2));

            m_pTask->setOptimizationMode(PSOTaskFoil::OptimizationMode::ModeB);
            m_pTask->setPlane3D(pPlane, wingIndex, sectionIndex);
            m_pTask->setInducedAlpha(m_CachedInducedAlpha);
        }
    } else {
        // Mode A: Standard 2D optimization
        m_pTask->setOptimizationMode(PSOTaskFoil::OptimizationMode::ModeA);
        m_CachedInducedAlpha = 0.0;
    }

    // Configure Constraints from dynamic list
    m_pTask->setConstraints(buildConstraints());

    updateUI(true);
    m_StatusLabel->setText("Optimizing...");
    m_ProgressBar->setRange(0, 0);
    m_ProgressBar->setValue(0);
    m_LogOutput->clear();
    log("Optimization started.");

    if(m_pGraphWt)
        m_pGraphWt->clearOutputInfo();

    m_pFitnessCurve->clear();
    if(m_pMetricCurve)
        m_pMetricCurve->clear();
    m_pGraph->invalidate();
    m_pGraph->resetLimits();
    m_pGraphWt->update();
    rebuildSectionPreview();

    m_RunActive = true;
    QCoreApplication::processEvents();

    // Reduce XFoil max iterations for optimization to avoid long hangs on bad particles
    m_OldIterLimit = XFoilTask::maxIterations();
    XFoilTask::setMaxIterations(30);

    // Reset rejection counts and show constraint labels
    m_pTask->resetRejectionCounts();
    for(ConstraintRow *row : m_ConstraintRows)
    {
        if(row->rejectCountLabel)
        {
            row->rejectCountLabel->setText("");
            row->rejectCountLabel->show();
        }
    }
    if(m_RejectionSummaryLabel)
        m_RejectionSummaryLabel->show();
    m_RejectionUpdateTimer->start();

    // Batch run loop
    m_TotalRuns = m_sbBatchRuns->value();
    m_GlobalBestFitness = 1e12;
    m_CurrentRun = 0;

    for(m_CurrentRun = 1; m_CurrentRun <= m_TotalRuns; ++m_CurrentRun)
    {
        // Check for cancellation between runs
        if(m_pTask && m_pTask->isCancelled()) break;

        // Log run start
        if(m_TotalRuns > 1)
            log(QString("=== Starting Run %1 of %2 ===").arg(m_CurrentRun).arg(m_TotalRuns));

        // Clear graph for new run (except first)
        if(m_CurrentRun > 1)
        {
            if(m_pFitnessCurve) m_pFitnessCurve->clear();
            m_pTask->reset();
        }

        // Run optimization
        m_pTask->onMakeParticleSwarm();
        m_pTask->onStartIterations();
        // At this point, m_BestParticle is set via OPTIM_END_EVENT handler

        // Compare with global best
        if(m_BestValid && m_BestParticle.fitness(0) < m_GlobalBestFitness)
        {
            m_GlobalBestFitness = m_BestParticle.fitness(0);
            m_GlobalBestParticle = m_BestParticle;
            if(m_TotalRuns > 1)
                log(QString("New global best! Fitness: %1").arg(m_GlobalBestFitness, 0, 'g', 6));
            updateCandidatePreview(m_GlobalBestParticle);
        }
    }

    // Final result: use global best (may differ from last run's best)
    if(m_TotalRuns > 1 && m_GlobalBestFitness < 1e10)
    {
        m_BestParticle = m_GlobalBestParticle;
        m_BestValid = true;
        log(QString("=== Batch complete. Best fitness: %1 ===").arg(m_GlobalBestFitness, 0, 'g', 6));
        updateCandidatePreview(m_BestParticle);
    }

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

    // Stop rejection update timer and show final stats
    if(m_RejectionUpdateTimer)
        m_RejectionUpdateTimer->stop();
    updateRejectionStats();

    if(m_pTask && m_pTask->isFinished()) return;
    updateUI(false);
}

void OptimizationPanel::updateUI(bool isRunning)
{
    m_RunButton->setEnabled(!isRunning);
    m_CancelButton->setEnabled(isRunning);
    m_ApplyBestButton->setEnabled(!isRunning && m_BestValid);
    m_PresetCombo->setEnabled(!isRunning);
    m_AddObjectiveBtn->setEnabled(!isRunning);
    for(auto *row : m_ObjectiveRows)
    {
        if(row && row->widget)
            row->widget->setEnabled(!isRunning);
    }
}

void OptimizationPanel::customEvent(QEvent *event)
{
    if(event->type() == OPTIM_SWARM_PROGRESS_EVENT)
    {
        OptimEvent *pEvent = static_cast<OptimEvent*>(event);
        if(pEvent) {
             m_ProgressBar->setRange(0, pEvent->iBest());
             m_ProgressBar->setValue(pEvent->iter());
             if(m_TotalRuns > 1)
                 m_StatusLabel->setText(QString("Run %1/%2 - Building Swarm %3/%4")
                     .arg(m_CurrentRun).arg(m_TotalRuns)
                     .arg(pEvent->iter()).arg(pEvent->iBest()));
             else
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
            if(m_TotalRuns > 1)
                m_StatusLabel->setText(QString("Run %1/%2 - Iteration %3/%4")
                    .arg(m_CurrentRun).arg(m_TotalRuns)
                    .arg(pEvent->iter()).arg(PSOTask::s_MaxIter));
            else
                m_StatusLabel->setText(QString("Iteration %1 / %2").arg(pEvent->iter()).arg(PSOTask::s_MaxIter));

            double bestFitness = pEvent->particle().fitness(0);

            // Only add to plot if fitness is reasonable (not a penalty value)
            // Penalty values are ~1e12 which make the plot unreadable
            constexpr double MAX_PLOT_FITNESS = 1e6;
            if(bestFitness < MAX_PLOT_FITNESS)
            {
                if(m_pFitnessCurve)
                    m_pFitnessCurve->appendPoint(pEvent->iter(), bestFitness);

                // Add metric value to right Y axis (use first objective's metric)
                if(m_pMetricCurve && !m_ObjectiveRows.isEmpty() && m_ObjectiveRows.first()->objectiveCombo)
                {
                    auto objType = static_cast<PSOTaskFoil::ObjectiveType>(
                        m_ObjectiveRows.first()->objectiveCombo->currentData().toInt());
                    double metricValue = objectiveMetricFromFitness(objType, bestFitness);
                    m_pMetricCurve->appendPoint(pEvent->iter(), metricValue);
                }
            }

            // Build objective label for display
            QString objLabel = "Multi-Objective";
            if(m_ObjectiveRows.size() == 1 && m_ObjectiveRows.first()->objectiveCombo)
                objLabel = m_ObjectiveRows.first()->objectiveCombo->currentText();

            const QString info = QString("Iter %1/%2\nFitness: %3\nObjective: %4")
                                     .arg(pEvent->iter())
                                     .arg(PSOTask::s_MaxIter)
                                     .arg(bestFitness, 0, 'g', 6)
                                     .arg(objLabel);
            if(m_pGraphWt)
                m_pGraphWt->setOutputInfo(info);

            m_pGraph->invalidate();
            m_pGraph->resetLimits();
            m_pGraphWt->update();

            updateCandidatePreview(pEvent->particle());

            // Update rejection stats every few iterations to avoid excessive updates
            if(pEvent->iter() % 5 == 0)
                updateRejectionStats();
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
             m_pGraph->invalidate();
             m_pGraph->resetLimits();
             m_pGraphWt->update();

             // Build objective label for display
             QString objLabel = "Multi-Objective";
             if(m_ObjectiveRows.size() == 1 && m_ObjectiveRows.first()->objectiveCombo)
                 objLabel = m_ObjectiveRows.first()->objectiveCombo->currentText();

             const QString info = QString("Iterations: %1\nFitness: %2\nObjective: %3")
                                      .arg(PSOTask::s_MaxIter)
                                      .arg(m_BestParticle.fitness(0), 0, 'g', 6)
                                      .arg(objLabel);
             if(m_pGraphWt)
                 m_pGraphWt->setOutputInfo(info);

             if(m_BestValid) {
                 m_StatusLabel->setText("Optimization finished. Click 'Apply' to create foil.");
                 log("Optimization finished. Apply to use the best result.");
             } else {
                 m_StatusLabel->setText("Optimization finished (no valid result).");
                 log("Warning: No converged solution found. All particles may have hit constraints or XFoil failed to converge.");
             }
        } else {
            m_BestValid = false;
            m_StatusLabel->setText("Optimization ended (no result).");
            log("Error: Optimization ended without a valid result.");
        }
        // Only disable UI after the last run of a batch
        if(m_CurrentRun >= m_TotalRuns)
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

void OptimizationPanel::updateOptimMarkersPreview()
{
    if(!m_pSectionView)
        return;

    if(!m_pFoil)
    {
        m_pSectionView->clearOptimMarkers();
        m_pSectionView->update();
        return;
    }

    // Create a temporary task to get the optimization markers
    PSOTaskFoil tempTask;
    tempTask.setFoil(m_pFoil);

    PSOTaskFoil::PresetType preset = static_cast<PSOTaskFoil::PresetType>(m_PresetCombo->currentData().toInt());
    tempTask.setPreset(preset);
    tempTask.setOptimizationPoints(m_sbOptimPoints->value());
    tempTask.setBoundsScale(m_sbBoundsScale->value());
    tempTask.setXBoundsScale(m_sbXBoundsScale->value());
    tempTask.setXMoveChordRange(m_sbXMoveMin->value() / 100.0, m_sbXMoveMax->value() / 100.0);
    tempTask.setSymmetric(m_cbSymmetric->isChecked());

    tempTask.initVariablesFromFoil();

    if(tempTask.nVariables() == 0)
    {
        m_pSectionView->clearOptimMarkers();
        m_pSectionView->update();
        return;
    }

    std::vector<std::pair<double, double>> ctrlPts;
    std::vector<std::tuple<double, double, double>> bounds;
    tempTask.getOptimMarkers(ctrlPts, bounds);
    m_pSectionView->setOptimMarkers(ctrlPts, bounds);
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

    // Update current control point positions to show movement
    std::vector<std::pair<double, double>> currentPts;
    m_pTask->getCurrentMarkers(particle, currentPts);
    m_pSectionView->setCurrentMarkers(currentPts);

    // Show foil properties instead of scale info
    QString foilInfo = QString("Thickness: %1%\nCamber: %2%\nX-Thick: %3%\nX-Camber: %4%")
        .arg(m_pPreviewFoil->maxThickness() * 100, 0, 'f', 2)
        .arg(m_pPreviewFoil->maxCamber() * 100, 0, 'f', 2)
        .arg(m_pPreviewFoil->xThickness() * 100, 0, 'f', 1)
        .arg(m_pPreviewFoil->xCamber() * 100, 0, 'f', 1);
    m_pSectionView->setOutputInfo(foilInfo);

    m_pSectionView->update();
}

void OptimizationPanel::onModeChanged()
{
    bool modeB = m_rbModeB->isChecked();
    m_pModeBOptions->setVisible(modeB);

    if (modeB) {
        populatePlaneList();
    }
}

void OptimizationPanel::onPlaneChanged(int index)
{
    PlaneXfl *pPlane = nullptr;
    if (index >= 0) {
        QString planeName = m_PlaneCombo->itemData(index).toString();
        pPlane = dynamic_cast<PlaneXfl*>(Objects3d::plane(planeName.toStdString()));
    }
    populateWingList(pPlane);
}

void OptimizationPanel::onWingChanged(int index)
{
    PlaneXfl *pPlane = nullptr;
    int planeIndex = m_PlaneCombo->currentIndex();
    if (planeIndex >= 0) {
        QString planeName = m_PlaneCombo->itemData(planeIndex).toString();
        pPlane = dynamic_cast<PlaneXfl*>(Objects3d::plane(planeName.toStdString()));
    }
    populateSectionList(pPlane, index);
}

void OptimizationPanel::populatePlaneList()
{
    m_PlaneCombo->blockSignals(true);
    m_PlaneCombo->clear();

    for (int i = 0; i < Objects3d::nPlanes(); ++i) {
        Plane *pPlane = Objects3d::planeAt(i);
        PlaneXfl *pPlaneXfl = dynamic_cast<PlaneXfl*>(pPlane);
        if (pPlaneXfl) {
            QString name = QString::fromStdString(pPlaneXfl->name());
            m_PlaneCombo->addItem(name, name);
        }
    }

    m_PlaneCombo->blockSignals(false);

    if (m_PlaneCombo->count() > 0) {
        m_PlaneCombo->setCurrentIndex(0);
        onPlaneChanged(0);
    }
}

void OptimizationPanel::populateWingList(PlaneXfl *pPlane)
{
    m_WingCombo->blockSignals(true);
    m_WingCombo->clear();

    if (pPlane) {
        // Check each potential wing
        const char* wingNames[] = {"Main Wing", "2nd Wing", "Elevator", "Fin"};
        for (int i = 0; i < 4; ++i) {
            WingXfl *pWing = pPlane->wing(i);
            if (pWing) {
                QString name = QString::fromStdString(pWing->name());
                if (name.isEmpty())
                    name = wingNames[i];
                m_WingCombo->addItem(name, i);
            }
        }
    }

    m_WingCombo->blockSignals(false);

    if (m_WingCombo->count() > 0) {
        m_WingCombo->setCurrentIndex(0);
        onWingChanged(0);
    }
}

void OptimizationPanel::populateSectionList(PlaneXfl *pPlane, int wingIndex)
{
    m_SectionCombo->blockSignals(true);
    m_SectionCombo->clear();

    if (pPlane && wingIndex >= 0) {
        WingXfl *pWing = pPlane->wing(wingIndex);
        if (pWing) {
            for (int i = 0; i < pWing->nSections(); ++i) {
                const WingSection &sec = pWing->section(i);
                QString label = QString("Section %1 (y=%2m)")
                                    .arg(i)
                                    .arg(sec.m_YPosition, 0, 'f', 3);
                m_SectionCombo->addItem(label, i);
            }
        }
    }

    m_SectionCombo->blockSignals(false);
}

// Constraint parameter definitions: name, default, step, max, min-allowed
// The operator (≥/≤) determines whether it's a min or max constraint
static const struct ConstraintDef {
    const char *name;
    double defaultVal;
    double step;
    double maxVal;
    double minVal;
    bool defaultIsMin; // true = default to ≥, false = default to ≤
} s_ConstraintDefs[] = {
    // Geometric (index 0-10)
    {"Thickness",       0.10,   0.001,  0.5,   0.0,   true},   // 0
    {"Camber",          0.02,   0.001,  0.2,  -0.2,   true},   // 1
    {"X Camber",        0.30,   0.01,   0.9,   0.0,   true},   // 2
    {"X Thickness",     0.30,   0.01,   0.9,   0.0,   true},   // 3
    {"LE Radius",       0.01,   0.001,  0.1,   0.0,   true},   // 4
    {"TE Gap",          0.002,  0.0005, 0.05,  0.0,   true},   // 5
    {"Wiggliness",      1.0,    0.1,    100.0, 0.0,   false},  // 6 (max by default)
    {"Section Modulus", 0.001,  0.0001, 1.0,   0.0,   true},   // 7
    {"Area",            0.05,   0.001,  0.5,   0.0,   true},   // 8
    {"Thick @80%",      0.02,   0.001,  0.2,   0.0,   true},   // 9 - thickness at 80% chord
    {"Thick @90%",      0.01,   0.001,  0.1,   0.0,   true},   // 10 - thickness at 90% chord
    // Aerodynamic (index 11-14)
    {"Cl",              0.5,    0.1,    3.0,  -1.0,   true},   // 11
    {"Cd",              0.02,   0.001,  0.5,   0.0,   false},  // 12 (max by default)
    {"Cm",             -0.1,    0.01,   0.5,  -0.5,   true},   // 13
    {"L/D",             20.0,   1.0,    200.0, 0.0,   true},   // 14
    // Pressure gradient constraints (index 15-18)
    // dCp/dx on upper surface - positive = adverse gradient (promotes separation)
    {"dCp/dx @10%",     2.0,    0.1,    20.0, -10.0,  false},  // 15 (max by default)
    {"dCp/dx @25%",     1.5,    0.1,    20.0, -10.0,  false},  // 16 (max by default)
    {"dCp/dx @50%",     1.0,    0.1,    20.0, -10.0,  false},  // 17 (max by default)
    {"dCp/dx @75%",     0.5,    0.1,    20.0, -10.0,  false},  // 18 (max by default)
};
static const int s_NumConstraintDefs = sizeof(s_ConstraintDefs) / sizeof(s_ConstraintDefs[0]);

void OptimizationPanel::addConstraintRow()
{
    auto *row = new ConstraintRow();
    row->widget = new QWidget(this);
    auto *layout = new QHBoxLayout(row->widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Parameter combo
    row->paramCombo = new QComboBox(this);
    for (int i = 0; i < s_NumConstraintDefs; ++i) {
        row->paramCombo->addItem(s_ConstraintDefs[i].name, i);
    }
    row->paramCombo->setFixedWidth(100);
    layout->addWidget(row->paramCombo);

    // Operator combo
    row->opCombo = new QComboBox(this);
    row->opCombo->addItem("≥", 0);  // Greater or equal (min constraint)
    row->opCombo->addItem("≤", 1);  // Less or equal (max constraint)
    row->opCombo->setFixedWidth(50);
    layout->addWidget(row->opCombo);

    // Reference checkbox
    row->refCheck = new QCheckBox("Ref", this);
    row->refCheck->setToolTip("Use base foil's value as reference");
    layout->addWidget(row->refCheck);

    // Value spinbox
    row->valueSpin = new QDoubleSpinBox(this);
    row->valueSpin->setDecimals(4);
    row->valueSpin->setFixedWidth(70);
    layout->addWidget(row->valueSpin);

    // Delete button
    row->deleteBtn = new QPushButton("×", this);
    row->deleteBtn->setFixedWidth(24);
    row->deleteBtn->setToolTip("Remove constraint");
    layout->addWidget(row->deleteBtn);

    // Rejection count label (hidden until optimization runs)
    row->rejectCountLabel = new QLabel("", this);
    row->rejectCountLabel->setFixedWidth(50);
    row->rejectCountLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->rejectCountLabel->setStyleSheet("");  // Will be styled during optimization
    row->rejectCountLabel->hide();  // Hidden until optimization runs
    layout->addWidget(row->rejectCountLabel);

    // Connect signals
    connect(row->paramCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, row](int) { onParamChanged(row); });
    connect(row->refCheck, &QCheckBox::toggled,
            this, [this, row](bool checked) { onRefCheckChanged(row, checked); });
    connect(row->deleteBtn, &QPushButton::clicked,
            this, [this, row]() { removeConstraintRow(row); });

    // Initialize with first parameter's defaults
    onParamChanged(row);

    m_ConstraintRows.append(row);
    m_ConstraintListLayout->addWidget(row->widget);
}

void OptimizationPanel::removeConstraintRow(ConstraintRow *row)
{
    if (!row) return;

    m_ConstraintListLayout->removeWidget(row->widget);
    m_ConstraintRows.removeOne(row);

    row->widget->deleteLater();
    delete row;
}

void OptimizationPanel::onParamChanged(ConstraintRow *row)
{
    if (!row || !row->paramCombo || !row->valueSpin || !row->opCombo) return;

    int defIdx = row->paramCombo->currentData().toInt();
    if (defIdx < 0 || defIdx >= s_NumConstraintDefs) return;

    const auto &def = s_ConstraintDefs[defIdx];
    row->valueSpin->setRange(def.minVal, def.maxVal);
    row->valueSpin->setSingleStep(def.step);

    // Set default operator based on parameter type
    row->opCombo->setCurrentIndex(def.defaultIsMin ? 0 : 1);

    // Reset reference checkbox and set default value
    row->refCheck->setChecked(false);
    row->valueSpin->setEnabled(true);
    row->valueSpin->setValue(def.defaultVal);
}

void OptimizationPanel::onRefCheckChanged(ConstraintRow *row, bool checked)
{
    if (!row || !row->paramCombo || !row->valueSpin) return;

    int defIdx = row->paramCombo->currentData().toInt();
    if (checked) {
        double refVal = getReferenceValue(defIdx);
        row->valueSpin->setValue(refVal);
        row->valueSpin->setEnabled(false);
    } else {
        row->valueSpin->setEnabled(true);
    }
}

double OptimizationPanel::getReferenceValue(int paramIndex) const
{
    if (!m_pFoil) return 0.0;

    switch (paramIndex) {
        case 0:  return m_pFoil->maxThickness();   // Thickness
        case 1:  return m_pFoil->maxCamber();      // Camber
        case 2:  return m_pFoil->xCamber();        // X Camber
        case 3:  return m_pFoil->xThickness();     // X Thickness
        case 4:  return m_pFoil->LERadius();       // LE Radius
        case 5:  return m_pFoil->TEGap();          // TE Gap
        case 6:  return m_pFoil->wiggliness();     // Wiggliness
        case 7: {                                   // Section Modulus (approx)
            double t = m_pFoil->maxThickness();
            return 0.12 * t * t;
        }
        case 8:  return m_pFoil->area();           // Area
        case 9:  return m_pFoil->thickness(0.80); // Thick @80%
        case 10: return m_pFoil->thickness(0.90); // Thick @90%
        // Aerodynamic parameters - no reference available (would need analysis)
        case 11: return 0.5;                       // Cl (no ref)
        case 12: return 0.01;                      // Cd (no ref)
        case 13: return -0.1;                      // Cm (no ref)
        case 14: return 50.0;                      // L/D (no ref)
        // Pressure gradient - no static reference (needs analysis)
        case 15: return 2.0;                       // dCp/dx @10%
        case 16: return 1.5;                       // dCp/dx @25%
        case 17: return 1.0;                       // dCp/dx @50%
        case 18: return 0.5;                       // dCp/dx @75%
        default: return 0.0;
    }
}

PSOTaskFoil::Constraints OptimizationPanel::buildConstraints() const
{
    PSOTaskFoil::Constraints c;
    c.enabled = !m_ConstraintRows.isEmpty();

    for (const auto *row : m_ConstraintRows) {
        if (!row || !row->paramCombo || !row->valueSpin || !row->opCombo) continue;

        int paramIdx = row->paramCombo->currentData().toInt();
        double val = row->valueSpin->value();
        bool isMin = (row->opCombo->currentIndex() == 0); // 0 = ≥, 1 = ≤

        // Map parameter + operator to constraint struct member
        switch (paramIdx) {
            case 0:  // Thickness
                if (isMin) c.minThickness = {val, true};
                else       c.maxThickness = {val, true};
                break;
            case 1:  // Camber
                if (isMin) c.minCamber = {val, true};
                else       c.maxCamber = {val, true};
                break;
            case 2:  // X Camber
                if (isMin) c.minXCamber = {val, true};
                else       c.maxXCamber = {val, true};
                break;
            case 3:  // X Thickness
                if (isMin) c.minXThickness = {val, true};
                else       c.maxXThickness = {val, true};
                break;
            case 4:  // LE Radius (only min makes sense)
                c.minLERadius = {val, true};
                break;
            case 5:  // TE Gap (only min makes sense)
                c.minTEThickness = {val, true};
                break;
            case 6:  // Wiggliness (only max makes sense)
                c.maxWiggliness = {val, true};
                break;
            case 7:  // Section Modulus (only min makes sense)
                c.minSectionModulus = {val, true};
                break;
            case 8:  // Area (only min makes sense)
                c.minArea = {val, true};
                break;
            case 9:  // Thick @80% (only min makes sense)
                c.minThickAt80 = {val, true};
                break;
            case 10: // Thick @90% (only min makes sense)
                c.minThickAt90 = {val, true};
                break;
            case 11: // Cl
                if (isMin) c.minCl = {val, true};
                else       c.maxCl = {val, true};
                break;
            case 12: // Cd
                if (isMin) c.minCd = {val, true};
                else       c.maxCd = {val, true};
                break;
            case 13: // Cm
                if (isMin) c.minCm = {val, true};
                else       c.maxCm = {val, true};
                break;
            case 14: // L/D (only min makes sense)
                c.minLD = {val, true};
                break;
            // Pressure gradient constraints (only max makes sense - limiting adverse gradient)
            case 15: // dCp/dx @10%
                c.maxDCpDxAt10 = {val, true};
                break;
            case 16: // dCp/dx @25%
                c.maxDCpDxAt25 = {val, true};
                break;
            case 17: // dCp/dx @50%
                c.maxDCpDxAt50 = {val, true};
                break;
            case 18: // dCp/dx @75%
                c.maxDCpDxAt75 = {val, true};
                break;
        }
    }

    return c;
}

PSOTaskFoil::ConstraintType OptimizationPanel::mapParamIndexToConstraintType(int paramIndex, int opIndex) const
{
    using CT = PSOTaskFoil::ConstraintType;
    bool isMin = (opIndex == 0);  // 0 = ≥ (min), 1 = ≤ (max)

    switch(paramIndex)
    {
        case 0:  // Thickness
            return isMin ? CT::MinThickness : CT::MaxThickness;
        case 1:  // Camber
            return isMin ? CT::MinCamber : CT::MaxCamber;
        case 2:  // X Camber
            return isMin ? CT::MinXCamber : CT::MaxXCamber;
        case 3:  // X Thickness
            return isMin ? CT::MinXThickness : CT::MaxXThickness;
        case 4:  // LE Radius (only min makes sense)
            return CT::MinLERadius;
        case 5:  // TE Gap
            return CT::MinTEThickness;
        case 6:  // Wiggliness (only max makes sense)
            return CT::MaxWiggliness;
        case 7:  // Section Modulus
            return CT::MinSectionModulus;
        case 8:  // Area
            return CT::MinArea;
        case 9:  // Thick @80%
            return CT::MinThickAt80;
        case 10: // Thick @90%
            return CT::MinThickAt90;
        case 11: // Cl
            return isMin ? CT::MinCl : CT::MaxCl;
        case 12: // Cd
            return isMin ? CT::MinCd : CT::MaxCd;
        case 13: // Cm
            return isMin ? CT::MinCm : CT::MaxCm;
        case 14: // L/D
            return CT::MinLD;
        case 15: // dCp/dx @10%
            return CT::MaxDCpDxAt10;
        case 16: // dCp/dx @25%
            return CT::MaxDCpDxAt25;
        case 17: // dCp/dx @50%
            return CT::MaxDCpDxAt50;
        case 18: // dCp/dx @75%
            return CT::MaxDCpDxAt75;
        default:
            return CT::COUNT;
    }
}

void OptimizationPanel::updateRejectionStats()
{
    if(!m_pTask)
        return;

    int totalEval = m_pTask->totalEvaluations();
    if(totalEval == 0)
        return;

    auto counts = m_pTask->allRejectionCounts();
    int totalRejections = 0;
    for(int c : counts)
        totalRejections += c;

    // Update per-constraint row labels
    for(ConstraintRow *row : m_ConstraintRows)
    {
        if(!row->paramCombo || !row->opCombo || !row->rejectCountLabel)
            continue;

        int paramIndex = row->paramCombo->currentData().toInt();
        int opIndex = row->opCombo->currentData().toInt();
        auto constraintType = mapParamIndexToConstraintType(paramIndex, opIndex);

        if(constraintType == PSOTaskFoil::ConstraintType::COUNT)
            continue;

        int count = m_pTask->rejectionCount(constraintType);
        if(count > 0)
        {
            double pct = 100.0 * count / totalEval;
            row->rejectCountLabel->setText(QString("%1").arg(count));

            // Color intensity based on rejection percentage
            // 0% = normal, 50%+ = bright red
            int intensity = std::min(255, static_cast<int>(pct * 5.1));
            row->rejectCountLabel->setStyleSheet(
                QString("color: rgb(%1, 0, 0); font-weight: bold;").arg(intensity));
        }
        else
        {
            row->rejectCountLabel->setText("");
            row->rejectCountLabel->setStyleSheet("");
        }
    }

    // Update summary label
    if(m_RejectionSummaryLabel)
    {
        // Find geometry failures
        int geomFailures = 0;
        for(int i = static_cast<int>(PSOTaskFoil::ConstraintType::GeomTEGap);
            i <= static_cast<int>(PSOTaskFoil::ConstraintType::GeomLETurnAngle); ++i)
        {
            geomFailures += counts[i];
        }

        int xfoilFailed = counts[static_cast<int>(PSOTaskFoil::ConstraintType::XFoilFailed)];

        QString summary;
        if(totalRejections > 0)
        {
            double rejectPct = 100.0 * totalRejections / totalEval;
            summary = QString("Rejections: %1/%2 (%3%)")
                          .arg(totalRejections).arg(totalEval).arg(rejectPct, 0, 'f', 1);

            if(geomFailures > 0)
                summary += QString("\nGeometry: %1").arg(geomFailures);
            if(xfoilFailed > 0)
                summary += QString("\nXFoil fail: %1").arg(xfoilFailed);
        }
        else
        {
            summary = QString("Evaluations: %1").arg(totalEval);
        }

        m_RejectionSummaryLabel->setText(summary);
    }
}

void OptimizationPanel::addObjectiveRow()
{
    auto *row = new ObjectiveRow();
    row->widget = new QWidget(this);
    auto *layout = new QHBoxLayout(row->widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Objective type combo
    row->objectiveCombo = new QComboBox(this);
    row->objectiveCombo->addItem("Min Cd", static_cast<int>(PSOTaskFoil::ObjectiveType::MinimizeCd));
    row->objectiveCombo->addItem("Max L/D", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizeLD));
    row->objectiveCombo->addItem("Max Cl", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizeCl));
    row->objectiveCombo->addItem("Min Cm", static_cast<int>(PSOTaskFoil::ObjectiveType::MinimizeCm));
    row->objectiveCombo->addItem("Max Power", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizePowerFactor));
    row->objectiveCombo->addItem("Max Endur", static_cast<int>(PSOTaskFoil::ObjectiveType::MaximizeEnduranceFactor));
    row->objectiveCombo->setFixedWidth(80);
    layout->addWidget(row->objectiveCombo, 2);

    // Target mode combo
    row->targetModeCombo = new QComboBox(this);
    row->targetModeCombo->addItem("α", static_cast<int>(PSOTaskFoil::TargetMode::Alpha));
    row->targetModeCombo->addItem("Cl", static_cast<int>(PSOTaskFoil::TargetMode::Cl));
    row->targetModeCombo->setFixedWidth(40);
    row->targetModeCombo->setCurrentIndex(1);  // Default to Cl
    layout->addWidget(row->targetModeCombo, 1);

    // Target value spinbox
    row->targetValueSpin = new QDoubleSpinBox(this);
    row->targetValueSpin->setRange(-20, 20);
    row->targetValueSpin->setValue(0.5);
    row->targetValueSpin->setDecimals(2);
    row->targetValueSpin->setFixedWidth(60);
    layout->addWidget(row->targetValueSpin, 1);

    // Reynolds number spinbox (per-objective)
    row->reynoldsSpin = new QDoubleSpinBox(this);
    row->reynoldsSpin->setRange(1e4, 1e8);
    row->reynoldsSpin->setValue(m_sbReynolds ? m_sbReynolds->value() * 1.0e6 : 1.0e6);
    row->reynoldsSpin->setDecimals(0);
    row->reynoldsSpin->setSingleStep(1e5);
    row->reynoldsSpin->setFixedWidth(70);
    row->reynoldsSpin->setToolTip("Reynolds number for this objective");
    layout->addWidget(row->reynoldsSpin, 1);

    // Weight spinbox
    row->weightSpin = new QDoubleSpinBox(this);
    row->weightSpin->setRange(0.0, 10.0);
    row->weightSpin->setValue(1.0);
    row->weightSpin->setSingleStep(0.1);
    row->weightSpin->setDecimals(2);
    row->weightSpin->setFixedWidth(50);
    row->weightSpin->setToolTip("Relative weight for this objective (higher = more important)");
    layout->addWidget(row->weightSpin, 1);

    // Delete button
    row->deleteBtn = new QPushButton("×", this);
    row->deleteBtn->setFixedWidth(24);
    row->deleteBtn->setToolTip("Remove objective");
    layout->addWidget(row->deleteBtn);

    // Connect signals
    connect(row->deleteBtn, &QPushButton::clicked,
            this, [this, row]() { removeObjectiveRow(row); });

    m_ObjectiveRows.append(row);
    m_ObjectiveListLayout->addWidget(row->widget);

    // Disable delete if only one row
    updateObjectiveDeleteButtons();
}

void OptimizationPanel::removeObjectiveRow(ObjectiveRow *row)
{
    if(!row || m_ObjectiveRows.size() <= 1)
        return;  // Must have at least one objective

    m_ObjectiveListLayout->removeWidget(row->widget);
    m_ObjectiveRows.removeOne(row);
    row->widget->deleteLater();
    delete row;

    updateObjectiveDeleteButtons();
}

void OptimizationPanel::updateObjectiveDeleteButtons()
{
    bool canDelete = m_ObjectiveRows.size() > 1;
    for(auto *row : m_ObjectiveRows)
    {
        if(row && row->deleteBtn)
            row->deleteBtn->setEnabled(canDelete);
    }
}

std::vector<PSOTaskFoil::ObjectiveSpec> OptimizationPanel::buildObjectiveSpecs() const
{
    std::vector<PSOTaskFoil::ObjectiveSpec> specs;

    for(const auto *row : m_ObjectiveRows)
    {
        if(!row || !row->objectiveCombo || !row->targetModeCombo ||
           !row->targetValueSpin || !row->reynoldsSpin || !row->weightSpin)
            continue;

        PSOTaskFoil::ObjectiveSpec spec;
        spec.type = static_cast<PSOTaskFoil::ObjectiveType>(
            row->objectiveCombo->currentData().toInt());
        spec.targetMode = static_cast<PSOTaskFoil::TargetMode>(
            row->targetModeCombo->currentData().toInt());
        spec.targetValue = row->targetValueSpin->value();
        spec.reynolds = row->reynoldsSpin->value();
        spec.weight = row->weightSpin->value();
        spec.enabled = true;

        specs.push_back(spec);
    }

    return specs;
}
