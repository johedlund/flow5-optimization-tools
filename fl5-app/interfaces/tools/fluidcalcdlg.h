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

class QDoubleSpinBox;
class QComboBox;
class QLabel;

class FluidCalcDlg : public QDialog
{
    Q_OBJECT
public:
    explicit FluidCalcDlg(QWidget *pParent = nullptr);

private slots:
    void onInputChanged();
    void onReChanged();
    void onClChanged();
    void onAltitudeChanged();
    void onFluidTypeChanged();

private:
    void updateCalculations(bool fromRe = false, bool fromCl = false);
    double dynamicViscosity(double tempK) const;
    
    QComboBox *m_pFluidTypeCombo;

    // Fluid props
    QDoubleSpinBox *m_pAltitudeSB;
    QDoubleSpinBox *m_pTemperatureSB;
    QDoubleSpinBox *m_pPressureSB;
    QDoubleSpinBox *m_pDensitySB;
    QDoubleSpinBox *m_pViscositySB;

    // Flow params
    QDoubleSpinBox *m_pSpeedSB;
    QDoubleSpinBox *m_pSpeedKnotsSB;
    QDoubleSpinBox *m_pChordSB;
    QDoubleSpinBox *m_pReynoldsSB;

    // Hydrofoil / Lift params
    QDoubleSpinBox *m_pMassSB;
    QDoubleSpinBox *m_pSurfaceAreaSB;
    QDoubleSpinBox *m_pClSB;
    QLabel *m_pClCategoryLabel;

    bool m_bBlockSignals = false;
};