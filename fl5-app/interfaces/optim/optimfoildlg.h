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

#include <QDialog>
#include <QFutureWatcher>
#include <QPointer>

#include <interfaces/optim/particle.h>

class Foil;
class Polar;
class PSOTaskFoil;
class QLabel;
class QProgressBar;
class QPushButton;
class QComboBox;
class GraphWt;
class Graph;
class Curve;
class CurveModel;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;

class OptimFoilDlg : public QDialog
{
    Q_OBJECT
public:

        explicit OptimFoilDlg(QWidget *pParent=nullptr);
        ~OptimFoilDlg();
        void initDialog(Foil *pFoil, Polar *pPolar);

    signals:
        void foilCreated(Foil *pFoil);

    protected:
        void customEvent(QEvent *event) override;

    private slots:
        void onRun();
        void onCancel();
        void onApplyBest();
        void onTaskFinished();

    private:
        void updateUI(bool isRunning);

        Foil *m_pFoil{nullptr};
        Polar *m_pPolar{nullptr};
        QPointer<PSOTaskFoil> m_pTask;
        QFutureWatcher<void> m_TaskWatcher;
        bool m_RunActive{false};
    Particle m_BestParticle;
    bool m_BestValid = false;

    GraphWt *m_pGraphWt;
    Graph *m_pGraph;
    CurveModel *m_pCurveModel;
    Curve *m_pFitnessCurve;

    QLabel *m_StatusLabel;
    QProgressBar *m_ProgressBar;
    QPushButton *m_RunButton;
    QPushButton *m_CancelButton;
    QPushButton *m_ApplyBestButton;
    QComboBox *m_PresetCombo;
    QComboBox *m_ObjectiveCombo;
    QComboBox *m_TargetModeCombo; // Fixed Alpha vs Fixed CL
    QDoubleSpinBox *m_TargetValueSpin;

    // Constraints
    QCheckBox *m_chkMinThickness; QDoubleSpinBox *m_sbMinThickness;
    QCheckBox *m_chkMaxThickness; QDoubleSpinBox *m_sbMaxThickness;
    QCheckBox *m_chkMinLERadius;  QDoubleSpinBox *m_sbMinLERadius;
    QCheckBox *m_chkMinTEGap;     QDoubleSpinBox *m_sbMinTEGap;
    QCheckBox *m_chkMaxWiggliness;QDoubleSpinBox *m_sbMaxWiggliness;
    QCheckBox *m_chkMinModulus;   QDoubleSpinBox *m_sbMinModulus;

    // Geometry
    QSpinBox *m_sbOptimPoints;
    QDoubleSpinBox *m_sbBoundsScale;
};
