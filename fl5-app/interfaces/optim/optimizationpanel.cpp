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
#include <QRadioButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>

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
    geoLayout->addRow("Points:", m_sbOptimPoints);

    m_sbBoundsScale = new QDoubleSpinBox(this);
    m_sbBoundsScale->setRange(0.1, 5.0);
    m_sbBoundsScale->setValue(1.0);
    m_sbBoundsScale->setSingleStep(0.1);
    m_sbBoundsScale->setSuffix("x");
    m_sbBoundsScale->setToolTip("Scales the search space for each variable.\n"
                                "Values > 1.0 allow larger deformations from the original foil.\n"
                                "Values < 1.0 constrain the optimization to the neighborhood of the original foil.");
    geoLayout->addRow("Bounds Scale:", m_sbBoundsScale);
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
    m_ObjectiveCombo->setToolTip("The primary goal of the optimization.");
    
    m_TargetModeCombo = new QComboBox(this);
    m_TargetModeCombo->addItem("Fixed Alpha", static_cast<int>(PSOTaskFoil::TargetMode::Alpha));
    m_TargetModeCombo->addItem("Fixed CL", static_cast<int>(PSOTaskFoil::TargetMode::Cl));
    m_TargetModeCombo->setToolTip("Defines the operating condition constraint.\n"
                                  "Fixed Alpha: Optimize at a constant Angle of Attack.\n"
                                  "Fixed CL: Optimize at a constant Lift Coefficient (alpha varies).");

    m_TargetValueSpin = new QDoubleSpinBox(this);
    m_TargetValueSpin->setRange(-20, 20); 
    m_TargetValueSpin->setValue(0.5);
    m_TargetValueSpin->setToolTip("The value for the selected Target Mode (Alpha in degrees or CL).");

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
    auto addConRow = [&](QString name, QCheckBox*& chk, QDoubleSpinBox*& sb, double val, double step, double max, QString tooltip) {
        chk = new QCheckBox(this); 
        chk->setToolTip(tooltip);
        sb = new QDoubleSpinBox(this);
        sb->setRange(0, max);
        sb->setValue(val);
        sb->setSingleStep(step);
        sb->setDecimals(4);
        sb->setEnabled(false);
        sb->setToolTip(tooltip);
        connect(chk, &QCheckBox::toggled, sb, &QDoubleSpinBox::setEnabled);
        
        QLabel* label = new QLabel(name);
        label->setToolTip(tooltip);
        conLayout->addWidget(label, row, 0);
        conLayout->addWidget(chk, row, 1);
        conLayout->addWidget(sb, row, 2);
        row++;
    };

    addConRow("Min Thickness (t/c)", m_chkMinThickness, m_sbMinThickness, 0.08, 0.001, 0.5,
              "Minimum maximum thickness relative to chord (t/c).");
    addConRow("Max Thickness (t/c)", m_chkMaxThickness, m_sbMaxThickness, 0.15, 0.001, 0.5,
              "Maximum maximum thickness relative to chord (t/c).");
    addConRow("Min LE Radius", m_chkMinLERadius, m_sbMinLERadius, 0.01, 0.001, 0.1,
              "Minimum Leading Edge radius relative to chord.\n"
              "Prevents sharp leading edges that cause separation spikes.");
    addConRow("Min TE Thickness", m_chkMinTEGap, m_sbMinTEGap, 0.002, 0.0005, 0.05,
              "Minimum Trailing Edge thickness (gap) relative to chord.\n"
              "Essential for structural integrity at the trailing edge.");
    addConRow("Max Wiggliness", m_chkMaxWiggliness, m_sbMaxWiggliness, 1.0, 0.1, 100.0,
              "Limits the variation of curvature to prevent wavy surfaces.");
    addConRow("Min Section Modulus (S/c³)", m_chkMinModulus, m_sbMinModulus, 0.001, 0.0001, 100000.0,
              "Minimum Section Modulus (approximated as 0.12 * t^2).\n"
              "Proxy for spar bending strength.");

    addConRow("Min Camber", m_chkMinCamber, m_sbMinCamber, 0.0, 0.001, 0.2,
              "Minimum maximum camber relative to chord.");
    addConRow("Max Camber", m_chkMaxCamber, m_sbMaxCamber, 0.1, 0.001, 0.2,
              "Maximum maximum camber relative to chord.");
    addConRow("Min X Camber", m_chkMinXCamber, m_sbMinXCamber, 0.1, 0.05, 0.9,
              "Minimum chordwise position of maximum camber (0.0=LE, 1.0=TE).");
    addConRow("Max X Camber", m_chkMaxXCamber, m_sbMaxXCamber, 0.5, 0.05, 0.9,
              "Maximum chordwise position of maximum camber (0.0=LE, 1.0=TE).");
    addConRow("Min X Thickness", m_chkMinXThickness, m_sbMinXThickness, 0.1, 0.05, 0.9,
              "Minimum chordwise position of maximum thickness (0.0=LE, 1.0=TE).");
    addConRow("Max X Thickness", m_chkMaxXThickness, m_sbMaxXThickness, 0.5, 0.05, 0.9,
              "Maximum chordwise position of maximum thickness (0.0=LE, 1.0=TE).");
    addConRow("Min Area", m_chkMinArea, m_sbMinArea, 0.01, 0.001, 0.5,
              "Minimum cross-sectional area relative to chord^2.\n"
              "Useful for internal volume or torsional stiffness.");

    addConRow("Min Cl", m_chkMinCl, m_sbMinCl, 0.0, 0.1, 5.0,
              "Minimum Lift Coefficient (Cl) constraint.");
    addConRow("Max Cl", m_chkMaxCl, m_sbMaxCl, 1.5, 0.1, 5.0,
              "Maximum Lift Coefficient (Cl) constraint.");
    addConRow("Min Cd", m_chkMinCd, m_sbMinCd, 0.0, 0.0001, 0.1,
              "Minimum Drag Coefficient (Cd) constraint (rarely used).");
    addConRow("Max Cd", m_chkMaxCd, m_sbMaxCd, 0.02, 0.001, 0.5,
              "Maximum Drag Coefficient (Cd) constraint.\n"
              "Discard foils with drag higher than this.");
    addConRow("Min Cm", m_chkMinCm, m_sbMinCm, -0.5, 0.01, 0.5,
              "Minimum Pitching Moment Coefficient (Cm).\n"
              "Usually negative; limits how nose-down the moment is.");
    addConRow("Max Cm", m_chkMaxCm, m_sbMaxCm, 0.0, 0.01, 0.5,
              "Maximum Pitching Moment Coefficient (Cm).");
    addConRow("Min L/D", m_chkMinLD, m_sbMinLD, 10.0, 1.0, 200.0,
              "Minimum Lift-to-Drag ratio constraint.");

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

    const QString objectiveLabel = m_ObjectiveCombo->currentText();
    const QString metricLabel = objectiveMetricLabel(objType);
    m_pGraph->setName(QString("Optimization Progress - %1").arg(objectiveLabel));
    m_pGraph->setYVariableList({QString("Fitness (%1)").arg(objectiveLabel), metricLabel});
    m_pGraph->setVariables(0, 0, 1);
    if(m_pFitnessCurve)
        m_pFitnessCurve->setName(QString("Fitness (%1)").arg(objectiveLabel));
    if(m_pMetricCurve)
        m_pMetricCurve->setName(metricLabel);

    m_BestValid = false;

    // Configure Target
    PSOTaskFoil::TargetMode targetMode = static_cast<PSOTaskFoil::TargetMode>(m_TargetModeCombo->currentData().toInt());
    double targetVal = m_TargetValueSpin->value();

    if (targetMode == PSOTaskFoil::TargetMode::Alpha)
        m_pTask->setTargetAlpha(targetVal);
    else
        m_pTask->setTargetCl(targetVal);

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

        // Run 3D analysis to get induced alpha
        log(QString("Mode B: Running 3D analysis for %1, wing %2, section %3...")
            .arg(planeName).arg(wingIndex).arg(sectionIndex));

        InducedAoAAdapter adapter;
        adapter.setPlane(pPlane, wingIndex, sectionIndex);
        adapter.setFlightConditions(targetVal, 30.0, 1.225, 1.5e-5);  // Use target alpha, default velocity
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

            log(QString("Mode B: Geometric AoA = %1 deg").arg(targetVal, 0, 'f', 2));
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
            if(m_pFitnessCurve)
                m_pFitnessCurve->appendPoint(pEvent->iter(), bestFitness);
            const PSOTaskFoil::ObjectiveType objType =
                static_cast<PSOTaskFoil::ObjectiveType>(m_ObjectiveCombo->currentData().toInt());
            const double metric = objectiveMetricFromFitness(objType, bestFitness);
            if(m_pMetricCurve)
                m_pMetricCurve->appendPoint(pEvent->iter(), metric);

            const QString metricLabel = objectiveMetricLabel(objType);
            const QString info = QString("Iter %1/%2\nFitness: %3\n%4: %5\nObjective: %6")
                                     .arg(pEvent->iter())
                                     .arg(PSOTask::s_MaxIter)
                                     .arg(bestFitness, 0, 'g', 6)
                                     .arg(metricLabel)
                                     .arg(metric, 0, 'g', 6)
                                     .arg(m_ObjectiveCombo ? m_ObjectiveCombo->currentText() : QString("Fitness"));
            if(m_pGraphWt)
                m_pGraphWt->setOutputInfo(info);

            m_pGraph->invalidate();
            m_pGraph->resetLimits();
            m_pGraphWt->update();

            if (pEvent->iter() % 5 == 0)
                updateCandidatePreview(pEvent->particle());
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

             const PSOTaskFoil::ObjectiveType objType =
                 static_cast<PSOTaskFoil::ObjectiveType>(m_ObjectiveCombo->currentData().toInt());
             const double metric = objectiveMetricFromFitness(objType, m_BestParticle.fitness(0));
             const QString metricLabel = objectiveMetricLabel(objType);
             const QString info = QString("Iterations: %1\nFitness: %2\n%3: %4\nObjective: %5")
                                      .arg(PSOTask::s_MaxIter)
                                      .arg(m_BestParticle.fitness(0), 0, 'g', 6)
                                      .arg(metricLabel)
                                      .arg(metric, 0, 'g', 6)
                                      .arg(m_ObjectiveCombo ? m_ObjectiveCombo->currentText() : QString("Fitness"));
             if(m_pGraphWt)
                 m_pGraphWt->setOutputInfo(info);

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
