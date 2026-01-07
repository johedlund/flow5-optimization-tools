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
class FoilWt;
class QTextEdit;

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

private:
    void updateUI(bool isRunning);
    void setupUI();
    void rebuildSectionPreview();
    void updateCandidatePreview(const Particle &particle);
    void clearPreviewFoils();
    void log(const QString &msg);

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

    // Run Parameters
    QDoubleSpinBox *m_sbReynolds;
    QDoubleSpinBox *m_sbMach;
    QDoubleSpinBox *m_sbNCrit;

    // Objectives
    QComboBox *m_ObjectiveCombo;
    QComboBox *m_TargetModeCombo;
    QDoubleSpinBox *m_TargetValueSpin;

    // Constraints (Table-like Grid)
    QCheckBox *m_chkMinThickness; QDoubleSpinBox *m_sbMinThickness;
    QCheckBox *m_chkMaxThickness; QDoubleSpinBox *m_sbMaxThickness;
    QCheckBox *m_chkMinLERadius;  QDoubleSpinBox *m_sbMinLERadius;
    QCheckBox *m_chkMinTEGap;     QDoubleSpinBox *m_sbMinTEGap;
    QCheckBox *m_chkMaxWiggliness;QDoubleSpinBox *m_sbMaxWiggliness;
    QCheckBox *m_chkMinModulus;   QDoubleSpinBox *m_sbMinModulus;

    // New Geometric Constraints
    QCheckBox *m_chkMinCamber;     QDoubleSpinBox *m_sbMinCamber;
    QCheckBox *m_chkMaxCamber;     QDoubleSpinBox *m_sbMaxCamber;
    QCheckBox *m_chkMinXCamber;    QDoubleSpinBox *m_sbMinXCamber;
    QCheckBox *m_chkMaxXCamber;    QDoubleSpinBox *m_sbMaxXCamber;
    QCheckBox *m_chkMinXThickness; QDoubleSpinBox *m_sbMinXThickness;
    QCheckBox *m_chkMaxXThickness; QDoubleSpinBox *m_sbMaxXThickness;
    QCheckBox *m_chkMinArea;       QDoubleSpinBox *m_sbMinArea;

    // Aerodynamic Constraints
    QCheckBox *m_chkMinCl; QDoubleSpinBox *m_sbMinCl;
    QCheckBox *m_chkMaxCl; QDoubleSpinBox *m_sbMaxCl;
    QCheckBox *m_chkMinCd; QDoubleSpinBox *m_sbMinCd;
    QCheckBox *m_chkMaxCd; QDoubleSpinBox *m_sbMaxCd;
    QCheckBox *m_chkMinCm; QDoubleSpinBox *m_sbMinCm;
    QCheckBox *m_chkMaxCm; QDoubleSpinBox *m_sbMaxCm;
    QCheckBox *m_chkMinLD; QDoubleSpinBox *m_sbMinLD;

    // Visualization
    GraphWt *m_pGraphWt;
    Graph *m_pGraph;
    CurveModel *m_pCurveModel;
    Curve *m_pFitnessCurve;

    FoilWt *m_pSectionView;
    Foil *m_pGhostFoil{nullptr};
    Foil *m_pPreviewFoil{nullptr};

    int m_OldIterLimit = 100;
};
