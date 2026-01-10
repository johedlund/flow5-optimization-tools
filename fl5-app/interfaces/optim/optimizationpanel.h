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

#pragma once

#include <QWidget>
#include <QPointer>
#include <QFutureWatcher>
#include <interfaces/optim/psotaskfoil.h>
#include <interfaces/optim/particle.h>

class Foil;
class Polar;
class PlaneXfl;
class GraphWt;
class Graph;
class Curve;
class CurveModel;
class QLabel;
class QProgressBar;
class QPushButton;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QRadioButton;
class QWidget;
class FoilWt;
class QTextEdit;
class QVBoxLayout;

// Dynamic constraint row widget
struct ConstraintRow {
    QWidget *widget{nullptr};
    QComboBox *paramCombo{nullptr};
    QComboBox *opCombo{nullptr};
    QCheckBox *refCheck{nullptr};
    QDoubleSpinBox *valueSpin{nullptr};
    QPushButton *deleteBtn{nullptr};
};

class OptimizationPanel : public QWidget
{
    Q_OBJECT
public:
    explicit OptimizationPanel(QWidget *pParent = nullptr);
    ~OptimizationPanel();

    void initPanel(Foil *pFoil, Polar *pPolar);

signals:
    void foilCreated(Foil *pFoil);
    void closeRequested();

protected:
    void customEvent(QEvent *event) override;

private slots:
    void onRun();
    void onCancel();
    void onApplyBest();
    void onTaskFinished();
    void onModeChanged();
    void onPlaneChanged(int index);
    void onWingChanged(int index);

private:
    void updateUI(bool isRunning);
    void setupUI();
    void rebuildSectionPreview();
    void updateCandidatePreview(const Particle &particle);
    void clearPreviewFoils();
    void log(const QString &msg);
    void populatePlaneList();
    void populateWingList(PlaneXfl *pPlane);
    void populateSectionList(PlaneXfl *pPlane, int wingIndex);

    // Dynamic constraints
    void addConstraintRow();
    void removeConstraintRow(ConstraintRow *row);
    void onParamChanged(ConstraintRow *row);
    void onRefCheckChanged(ConstraintRow *row, bool checked);
    double getReferenceValue(int paramIndex) const;
    PSOTaskFoil::Constraints buildConstraints() const;

    // Optimization preview
    void updateOptimMarkersPreview();

    Foil *m_pFoil{nullptr};
    Polar *m_pPolar{nullptr};
    QPointer<PSOTaskFoil> m_pTask;
    QFutureWatcher<void> m_TaskWatcher;
    bool m_RunActive{false};
    Particle m_BestParticle;
    bool m_BestValid = false;

    // UI Components
    QLabel *m_StatusLabel;
    QProgressBar *m_ProgressBar;
    QTextEdit *m_LogOutput;
    
    QPushButton *m_RunButton;
    QPushButton *m_CancelButton;
    QPushButton *m_ApplyBestButton;
    QPushButton *m_CloseButton;

    // Configuration Inputs
    QComboBox *m_PresetCombo;
    QSpinBox *m_sbOptimPoints;
    QDoubleSpinBox *m_sbBoundsScale;
    QDoubleSpinBox *m_sbXMoveMin;
    QDoubleSpinBox *m_sbXMoveMax;
    QCheckBox *m_cbSymmetric;

    // Run Parameters
    QSpinBox *m_sbMaxIter;
    QDoubleSpinBox *m_sbReynolds;
    QDoubleSpinBox *m_sbMach;
    QDoubleSpinBox *m_sbNCrit;

    // Mode B (3D coupling)
    QRadioButton *m_rbModeA;
    QRadioButton *m_rbModeB;
    QWidget *m_pModeBOptions;
    QComboBox *m_PlaneCombo;
    QComboBox *m_WingCombo;
    QComboBox *m_SectionCombo;
    double m_CachedInducedAlpha{0.0};

    // Objectives
    QComboBox *m_ObjectiveCombo;
    QComboBox *m_TargetModeCombo;
    QDoubleSpinBox *m_TargetValueSpin;

    // Dynamic Constraints
    QVBoxLayout *m_ConstraintListLayout{nullptr};
    QPushButton *m_AddConstraintBtn{nullptr};
    QList<ConstraintRow*> m_ConstraintRows;

    // Visualization
    GraphWt *m_pGraphWt;
    Graph *m_pGraph;
    CurveModel *m_pCurveModel;
    Curve *m_pFitnessCurve;
    Curve *m_pMetricCurve;

    FoilWt *m_pSectionView;
    Foil *m_pGhostFoil{nullptr};
    Foil *m_pPreviewFoil{nullptr};

    int m_OldIterLimit = 100;
};
