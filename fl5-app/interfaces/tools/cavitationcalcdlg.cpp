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

#include "cavitationcalcdlg.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <cmath>
#include <map>

CavitationCalcDlg::CavitationCalcDlg(QWidget *pParent)
    : QDialog(pParent)
{
    setWindowTitle("Cavitation Analysis Tool");
    setMinimumWidth(400);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Environmental Group ---
    auto *envGroup = new QGroupBox("Environmental Conditions", this);
    auto *envLayout = new QFormLayout(envGroup);

    m_pDepthSB = new QDoubleSpinBox(this);
    m_pDepthSB->setRange(0, 100);
    m_pDepthSB->setValue(0.5);
    m_pDepthSB->setSuffix(" m");
    envLayout->addRow("Submersion Depth:", m_pDepthSB);

    m_pTemperatureSB = new QDoubleSpinBox(this);
    m_pTemperatureSB->setRange(0, 40);
    m_pTemperatureSB->setValue(15.0);
    m_pTemperatureSB->setSuffix(" °C");
    envLayout->addRow("Water Temperature:", m_pTemperatureSB);

    m_pAtmPressureSB = new QDoubleSpinBox(this);
    m_pAtmPressureSB->setRange(50000, 110000);
    m_pAtmPressureSB->setValue(101325);
    m_pAtmPressureSB->setSuffix(" Pa");
    m_pAtmPressureSB->setDecimals(0);
    envLayout->addRow("Atmospheric Pressure:", m_pAtmPressureSB);

    mainLayout->addWidget(envGroup);

    // --- Foil Loading Group ---
    auto *foilGroup = new QGroupBox("Foil Operating Point", this);
    auto *foilLayout = new QFormLayout(foilGroup);

    m_pMinCpSB = new QDoubleSpinBox(this);
    m_pMinCpSB->setRange(-20.0, 0.0);
    m_pMinCpSB->setValue(-1.0);
    m_pMinCpSB->setDecimals(3);
    foilLayout->addRow("Minimum Cp (Suction Peak):", m_pMinCpSB);

    m_pOpSpeedSB = new QDoubleSpinBox(this);
    m_pOpSpeedSB->setRange(0.1, 100.0);
    m_pOpSpeedSB->setValue(10.0);
    m_pOpSpeedSB->setSuffix(" m/s");
    foilLayout->addRow("Operating Speed:", m_pOpSpeedSB);

    mainLayout->addWidget(foilGroup);

    // --- Results Group ---
    auto *resGroup = new QGroupBox("Analysis Results", this);
    auto *resLayout = new QVBoxLayout(resGroup);

    m_pInceptionSpeedLabel = new QLabel(this);
    m_pSigmaLabel = new QLabel(this);
    m_pSafetyMarginLabel = new QLabel(this);
    m_pStatusLabel = new QLabel(this);
    m_pStatusLabel->setStyleSheet("font-weight: bold;");

    resLayout->addWidget(m_pInceptionSpeedLabel);
    resLayout->addWidget(m_pSigmaLabel);
    resLayout->addWidget(m_pSafetyMarginLabel);
    resLayout->addWidget(m_pStatusLabel);

    mainLayout->addWidget(resGroup);

    auto *closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeButton);

    connect(m_pDepthSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CavitationCalcDlg::onInputChanged);
    connect(m_pTemperatureSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CavitationCalcDlg::onInputChanged);
    connect(m_pAtmPressureSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CavitationCalcDlg::onInputChanged);
    connect(m_pMinCpSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CavitationCalcDlg::onInputChanged);
    connect(m_pOpSpeedSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CavitationCalcDlg::onInputChanged);

    updateCalculations();
}

void CavitationCalcDlg::onInputChanged()
{
    if (m_bBlockSignals) return;
    updateCalculations();
}

double CavitationCalcDlg::getVaporPressure(double tempC) const
{
    // Foilinizer data points
    static const std::map<double, double> vp_data = {
        {0, 611}, {5, 872}, {10, 1228}, {15, 1705}, {20, 2338},
        {25, 3169}, {30, 4243}, {35, 5627}, {40, 7381}
    };

    if (tempC <= vp_data.begin()->first) return vp_data.begin()->second;
    if (tempC >= vp_data.rbegin()->first) return vp_data.rbegin()->second;

    auto it = vp_data.lower_bound(tempC);
    auto prev = std::prev(it);

    double t1 = prev->first;
    double p1 = prev->second;
    double t2 = it->first;
    double p2 = it->second;

    return p1 + (p2 - p1) * (tempC - t1) / (t2 - t1);
}

double CavitationCalcDlg::getWaterDensity(double tempC) const
{
    // Foilinizer formula: rho = 1000 * (1 - 0.00004 * (T - 4)^2) + 25 (for seawater)
    return 1000.0 * (1.0 - 0.00004 * std::pow(tempC - 4.0, 2)) + 25.0;
}

void CavitationCalcDlg::updateCalculations()
{
    m_bBlockSignals = true;

    double depth = m_pDepthSB->value();
    double temp = m_pTemperatureSB->value();
    double p_atm = m_pAtmPressureSB->value();
    double min_cp = m_pMinCpSB->value();
    double v_op = m_pOpSpeedSB->value();

    double rho = getWaterDensity(temp);
    double p_vapor = getVaporPressure(temp);
    double g = 9.81;

    // P_static = P_atm + rho * g * depth
    double p_static = p_atm + rho * g * depth;

    // V_inception = sqrt( 2 * (P_static - P_vapor) / (rho * (-min_cp)) )
    double v_inception = 0;
    if (min_cp < 0)
    {
        v_inception = std::sqrt(2.0 * (p_static - p_vapor) / (rho * (-min_cp)));
        m_pInceptionSpeedLabel->setText(QString("Inception Speed: %1 m/s (%2 kn)")
                                        .arg(v_inception, 0, 'f', 2)
                                        .arg(v_inception / 0.514444, 0, 'f', 2));
    }
    else
    {
        m_pInceptionSpeedLabel->setText("Inception Speed: ∞ (No suction)");
    }

    // Sigma = (P_static - P_vapor) / (0.5 * rho * V^2)
    double sigma = (p_static - p_vapor) / (0.5 * rho * v_op * v_op);
    m_pSigmaLabel->setText(QString("Cavitation Number (σ): %1").arg(sigma, 0, 'f', 3));

    // Safety Margin = V_inception / V_op
    if (v_op > 0 && v_inception > 0)
    {
        double margin = v_inception / v_op;
        m_pSafetyMarginLabel->setText(QString("Safety Margin (V_inc / V_op): %1").arg(margin, 0, 'f', 2));

        if (margin < 1.0)
        {
            m_pStatusLabel->setText("STATUS: CAVITATING");
            m_pStatusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
        else if (margin < 1.2)
        {
            m_pStatusLabel->setText("STATUS: RISK OF CAVITATION");
            m_pStatusLabel->setStyleSheet("color: orange; font-weight: bold;");
        }
        else
        {
            m_pStatusLabel->setText("STATUS: SAFE");
            m_pStatusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
    }
    else
    {
        m_pSafetyMarginLabel->setText("Safety Margin: -");
        m_pStatusLabel->setText("STATUS: -");
    }

    m_bBlockSignals = false;
}
