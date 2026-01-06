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
class QLabel;

class CavitationCalcDlg : public QDialog
{
    Q_OBJECT
public:
    explicit CavitationCalcDlg(QWidget *pParent = nullptr);

private slots:
    void onInputChanged();

private:
    void updateCalculations();
    double getVaporPressure(double tempC) const;
    double getWaterDensity(double tempC) const;

    // Environmental inputs
    QDoubleSpinBox *m_pDepthSB;
    QDoubleSpinBox *m_pTemperatureSB;
    QDoubleSpinBox *m_pAtmPressureSB;

    // Operating inputs
    QDoubleSpinBox *m_pMinCpSB;
    QDoubleSpinBox *m_pOpSpeedSB;

    // Outputs
    QLabel *m_pInceptionSpeedLabel;
    QLabel *m_pSigmaLabel;
    QLabel *m_pSafetyMarginLabel;
    QLabel *m_pStatusLabel;

    bool m_bBlockSignals = false;
};
