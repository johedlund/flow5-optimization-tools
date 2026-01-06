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

#include "fluidcalcdlg.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <cmath>

FluidCalcDlg::FluidCalcDlg(QWidget *pParent)
    : QDialog(pParent)
{
    setWindowTitle("Fluid & Operating Conditions Calculator");
    setMinimumWidth(450);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Fluid Selection ---
    auto *topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("Fluid Type:"));
    m_pFluidTypeCombo = new QComboBox(this);
    m_pFluidTypeCombo->addItem("Air (ISA model)");
    m_pFluidTypeCombo->addItem("Water (Seawater)");
    topLayout->addWidget(m_pFluidTypeCombo);
    mainLayout->addLayout(topLayout);

    // --- Fluid Properties Group ---
    auto *fluidGroup = new QGroupBox("Fluid Properties", this);
    auto *fluidLayout = new QFormLayout(fluidGroup);

    m_pAltitudeSB = new QDoubleSpinBox(this);
    m_pAltitudeSB->setRange(-1000, 20000);
    m_pAltitudeSB->setValue(0);
    m_pAltitudeSB->setSuffix(" m");
    m_pAltitudeSB->setDecimals(1);
    fluidLayout->addRow("Altitude/Depth:", m_pAltitudeSB);

    m_pTemperatureSB = new QDoubleSpinBox(this);
    m_pTemperatureSB->setRange(-100, 100);
    m_pTemperatureSB->setValue(15.0);
    m_pTemperatureSB->setSuffix(" °C");
    fluidLayout->addRow("Temperature:", m_pTemperatureSB);

    m_pPressureSB = new QDoubleSpinBox(this);
    m_pPressureSB->setRange(0, 1000000);
    m_pPressureSB->setValue(101325);
    m_pPressureSB->setSuffix(" Pa");
    m_pPressureSB->setDecimals(0);
    fluidLayout->addRow("Pressure:", m_pPressureSB);

    m_pDensitySB = new QDoubleSpinBox(this);
    m_pDensitySB->setRange(0, 2000);
    m_pDensitySB->setValue(1.225);
    m_pDensitySB->setSuffix(" kg/m³");
    m_pDensitySB->setDecimals(3);
    fluidLayout->addRow("Density:", m_pDensitySB);

    m_pViscositySB = new QDoubleSpinBox(this);
    m_pViscositySB->setRange(0, 1e-2);
    m_pViscositySB->setValue(1.46e-5);
    m_pViscositySB->setSuffix(" m²/s");
    m_pViscositySB->setDecimals(8);
    m_pViscositySB->setReadOnly(true);
    fluidLayout->addRow("Kin. Viscosity:", m_pViscositySB);

    mainLayout->addWidget(fluidGroup);

    // --- Flow Parameters Group ---
    auto *flowGroup = new QGroupBox("Flow Parameters", this);
    auto *flowLayout = new QFormLayout(flowGroup);

    auto *speedLayout = new QHBoxLayout();
    m_pSpeedSB = new QDoubleSpinBox(this);
    m_pSpeedSB->setRange(0, 2000);
    m_pSpeedSB->setValue(10.0);
    m_pSpeedSB->setSuffix(" m/s");
    speedLayout->addWidget(m_pSpeedSB);

    m_pSpeedKnotsSB = new QDoubleSpinBox(this);
    m_pSpeedKnotsSB->setRange(0, 1000);
    m_pSpeedKnotsSB->setValue(19.44);
    m_pSpeedKnotsSB->setSuffix(" kn");
    speedLayout->addWidget(m_pSpeedKnotsSB);
    flowLayout->addRow("Speed:", speedLayout);

    m_pChordSB = new QDoubleSpinBox(this);
    m_pChordSB->setRange(0.001, 100);
    m_pChordSB->setValue(1.0);
    m_pChordSB->setSuffix(" m");
    m_pChordSB->setDecimals(3);
    flowLayout->addRow("Chord:", m_pChordSB);

    m_pReynoldsSB = new QDoubleSpinBox(this);
    m_pReynoldsSB->setRange(0, 1e10);
    m_pReynoldsSB->setValue(1e6);
    m_pReynoldsSB->setDecimals(0);
    m_pReynoldsSB->setSingleStep(10000);
    flowLayout->addRow("Reynolds:", m_pReynoldsSB);

    mainLayout->addWidget(flowGroup);

    // --- Operating Lift Group ---
    auto *liftGroup = new QGroupBox("Operating Lift / Cl", this);
    auto *liftLayout = new QFormLayout(liftGroup);

    m_pMassSB = new QDoubleSpinBox(this);
    m_pMassSB->setRange(0, 1e7);
    m_pMassSB->setValue(1000);
    m_pMassSB->setSuffix(" kg");
    liftLayout->addRow("Operating Mass:", m_pMassSB);

    m_pSurfaceAreaSB = new QDoubleSpinBox(this);
    m_pSurfaceAreaSB->setRange(0.001, 10000);
    m_pSurfaceAreaSB->setValue(2.0);
    m_pSurfaceAreaSB->setSuffix(" m²");
    m_pSurfaceAreaSB->setDecimals(3);
    liftLayout->addRow("Surface Area:", m_pSurfaceAreaSB);

    m_pClSB = new QDoubleSpinBox(this);
    m_pClSB->setRange(-5, 5);
    m_pClSB->setValue(0.4);
    m_pClSB->setDecimals(4);
    liftLayout->addRow("Lift Coeff (Cl):", m_pClSB);

    m_pClCategoryLabel = new QLabel("Regime: Normal", this);
    liftLayout->addRow(m_pClCategoryLabel);

    mainLayout->addWidget(liftGroup);

    auto *closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeButton);

    connect(m_pFluidTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FluidCalcDlg::onFluidTypeChanged);
    connect(m_pAltitudeSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onAltitudeChanged);
    connect(m_pTemperatureSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pPressureSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pDensitySB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pSpeedSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pSpeedKnotsSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pChordSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pReynoldsSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onReChanged);
    connect(m_pMassSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pSurfaceAreaSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onInputChanged);
    connect(m_pClSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FluidCalcDlg::onClChanged);

    onFluidTypeChanged();
}

void FluidCalcDlg::onFluidTypeChanged()
{
    m_bBlockSignals = true;
    if (m_pFluidTypeCombo->currentIndex() == 0) // Air
    {
        m_pAltitudeSB->setEnabled(true);
        m_pDensitySB->setValue(1.225);
        m_pTemperatureSB->setValue(15.0);
        m_pPressureSB->setValue(101325);
    }
    else // Water
    {
        m_pAltitudeSB->setEnabled(false);
        m_pDensitySB->setValue(1025.0);
        m_pTemperatureSB->setValue(15.0);
        m_pPressureSB->setValue(101325);
    }
    m_bBlockSignals = false;
    updateCalculations();
}

void FluidCalcDlg::onAltitudeChanged()
{
    if (m_bBlockSignals || m_pFluidTypeCombo->currentIndex() != 0) return;
    m_bBlockSignals = true;

    double alt = m_pAltitudeSB->value();
    double T0 = 288.15; // K
    double P0 = 101325; // Pa
    double L = 0.0065;  // K/m
    double R = 287.05;  // J/(kg·K)
    double g = 9.80665; // m/s²

    double T = T0 - L * alt;
    double P = P0 * std::pow(1 - L * alt / T0, g / (R * L));

    m_pTemperatureSB->setValue(T - 273.15);
    m_pPressureSB->setValue(P);

    m_bBlockSignals = false;
    onInputChanged();
}

void FluidCalcDlg::onInputChanged()
{
    if (m_bBlockSignals) return;
    updateCalculations(false, false);
}

void FluidCalcDlg::onReChanged()
{
    if (m_bBlockSignals) return;
    updateCalculations(true, false);
}

void FluidCalcDlg::onClChanged()
{
    if (m_bBlockSignals) return;
    updateCalculations(false, true);
}

void FluidCalcDlg::updateCalculations(bool fromRe, bool fromCl)
{
    m_bBlockSignals = true;

    double T = m_pTemperatureSB->value() + 273.15; // K
    double P = m_pPressureSB->value();
    double R = 287.05;

    if (m_pFluidTypeCombo->currentIndex() == 0) // Air
    {
        if (sender() == m_pDensitySB)
        {
            P = m_pDensitySB->value() * R * T;
            m_pPressureSB->setValue(P);
        }
        else
        {
            double rho = P / (R * T);
            m_pDensitySB->setValue(rho);
        }
        double mu = dynamicViscosity(T);
        double nu = mu / m_pDensitySB->value();
        m_pViscositySB->setValue(nu);
    }
    else // Water - Simplified constant properties for now or basic temp empirical
    {
        // Kinematic viscosity of water vs T approx: nu = 1.79e-6 / (1 + 0.0337*T + 0.000221*T^2)
        double t_c = m_pTemperatureSB->value();
        double nu = 1.79e-6 / (1.0 + 0.0337 * t_c + 0.000221 * t_c * t_c);
        m_pViscositySB->setValue(nu);
    }

    double V = m_pSpeedSB->value();
    if (sender() == m_pSpeedKnotsSB)
    {
        V = m_pSpeedKnotsSB->value() * 0.514444;
        m_pSpeedSB->setValue(V);
    }
    else
    {
        m_pSpeedKnotsSB->setValue(V / 0.514444);
    }

    double L = m_pChordSB->value();
    double nu = m_pViscositySB->value();
    double rho = m_pDensitySB->value();

    if (fromRe)
    {
        double Re = m_pReynoldsSB->value();
        if (L > 0 && nu > 0)
        {
            V = Re * nu / L;
            m_pSpeedSB->setValue(V);
            m_pSpeedKnotsSB->setValue(V / 0.514444);
        }
    }
    else
    {
        if (nu > 0)
        {
            double Re = V * L / nu;
            m_pReynoldsSB->setValue(Re);
        }
    }

    // Lift / Cl
    double S = m_pSurfaceAreaSB->value();
    double mass = m_pMassSB->value();
    double lift = mass * 9.81;

    if (fromCl)
    {
        double cl = m_pClSB->value();
        if (cl != 0 && S > 0 && rho > 0)
        {
            // Cl = L / (0.5 * rho * V^2 * S) => V = sqrt( L / (0.5 * rho * S * Cl) )
            double v2 = lift / (0.5 * rho * S * cl);
            if (v2 > 0)
            {
                V = std::sqrt(v2);
                m_pSpeedSB->setValue(V);
                m_pSpeedKnotsSB->setValue(V / 0.514444);
                // Re-calculate Re since V changed
                if (nu > 0) m_pReynoldsSB->setValue(V * L / nu);
            }
        }
    }
    else
    {
        if (V > 0 && S > 0)
        {
            double cl = lift / (0.5 * rho * V * V * S);
            m_pClSB->setValue(cl);
        }
    }

    // Category
    double cl_val = m_pClSB->value();
    QString cat = "Regime: ";
    if (cl_val < 0.1) cat += "Very Low";
    else if (cl_val < 0.8) cat += "Normal";
    else if (cl_val < 1.5) cat += "High";
    else cat += "Extreme";
    m_pClCategoryLabel->setText(cat);

    m_bBlockSignals = false;
}

double FluidCalcDlg::dynamicViscosity(double T) const
{
    double mu0 = 1.716e-5;
    double T0 = 273.15;
    double S = 110.4;
    return mu0 * std::pow(T / T0, 1.5) * (T0 + S) / (T + S);
}