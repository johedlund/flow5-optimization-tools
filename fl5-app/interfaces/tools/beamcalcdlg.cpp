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

#include "beamcalcdlg.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <cmath>

BeamCalcDlg::BeamCalcDlg(QWidget *pParent)
    : QDialog(pParent)
{
    setWindowTitle("Beam & Spar Structural Calculator");
    setMinimumWidth(450);

    initMaterials();

    auto *mainLayout = new QVBoxLayout(this);

    // --- Loading Group ---
    auto *loadGroup = new QGroupBox("Loading & Geometry", this);
    auto *loadLayout = new QFormLayout(loadGroup);

    m_pDistCombo = new QComboBox(this);
    m_pDistCombo->addItem("Elliptical");
    m_pDistCombo->addItem("Uniform");
    m_pDistCombo->addItem("Triangular");
    loadLayout->addRow("Load Distribution:", m_pDistCombo);

    m_pTotalMassSB = new QDoubleSpinBox(this);
    m_pTotalMassSB->setRange(0.1, 100000);
    m_pTotalMassSB->setValue(1000.0);
    m_pTotalMassSB->setSuffix(" kg");
    loadLayout->addRow("Aircraft Mass:", m_pTotalMassSB);

    m_pLoadFactorSB = new QDoubleSpinBox(this);
    m_pLoadFactorSB->setRange(1.0, 20.0);
    m_pLoadFactorSB->setValue(3.0);
    m_pLoadFactorSB->setSuffix(" g");
    loadLayout->addRow("Design Load Factor:", m_pLoadFactorSB);

    m_pSpanSB = new QDoubleSpinBox(this);
    m_pSpanSB->setRange(0.1, 100.0);
    m_pSpanSB->setValue(2.0);
    m_pSpanSB->setSuffix(" m (half-span)");
    loadLayout->addRow("Span (cantilever):", m_pSpanSB);

    m_pChordSB = new QDoubleSpinBox(this);
    m_pChordSB->setRange(0.01, 10.0);
    m_pChordSB->setValue(0.2);
    m_pChordSB->setSuffix(" m");
    loadLayout->addRow("Root Chord:", m_pChordSB);

    mainLayout->addWidget(loadGroup);

    // --- Material & Safety ---
    auto *matGroup = new QGroupBox("Material & Safety", this);
    auto *matLayout = new QFormLayout(matGroup);

    m_pMaterialCombo = new QComboBox(this);
    for (const auto &mat : m_Materials)
        m_pMaterialCombo->addItem(QString::fromStdString(mat.name));
    matLayout->addRow("Spar Material:", m_pMaterialCombo);

    m_pSafetyFactorSB = new QDoubleSpinBox(this);
    m_pSafetyFactorSB->setRange(1.0, 10.0);
    m_pSafetyFactorSB->setValue(2.0);
    matLayout->addRow("Safety Factor:", m_pSafetyFactorSB);

    mainLayout->addWidget(matGroup);

    // --- Section Group ---
    auto *sectionGroup = new QGroupBox("Section Modeling", this);
    auto *sectionLayout = new QFormLayout(sectionGroup);

    m_pSparWidthSB = new QDoubleSpinBox(this);
    m_pSparWidthSB->setRange(1.0, 500.0);
    m_pSparWidthSB->setValue(20.0);
    m_pSparWidthSB->setSuffix(" mm");
    sectionLayout->addRow("Box/Cap Width:", m_pSparWidthSB);

    m_pSparHeightSB = new QDoubleSpinBox(this);
    m_pSparHeightSB->setRange(1.0, 500.0);
    m_pSparHeightSB->setValue(15.0);
    m_pSparHeightSB->setSuffix(" mm");
    sectionLayout->addRow("Box/Cap Height:", m_pSparHeightSB);

    mainLayout->addWidget(sectionGroup);

    // --- Results Group ---
    auto *resGroup = new QGroupBox("Structural Requirements (Root)", this);
    auto *resLayout = new QVBoxLayout(resGroup);

    m_pMaxMomentLabel = new QLabel(this);
    m_pRequiredSLabel = new QLabel(this);
    m_pRequiredAreaLabel = new QLabel(this);
    m_pRequiredTCLabel = new QLabel(this);
    m_pEstimatedWeightLabel = new QLabel(this);

    resLayout->addWidget(m_pMaxMomentLabel);
    resLayout->addWidget(m_pRequiredSLabel);
    resLayout->addWidget(m_pRequiredAreaLabel);
    resLayout->addWidget(m_pRequiredTCLabel);
    resLayout->addWidget(m_pEstimatedWeightLabel);

    mainLayout->addWidget(resGroup);

    auto *closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeButton);

    connect(m_pDistCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BeamCalcDlg::onInputChanged);
    connect(m_pTotalMassSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamCalcDlg::onInputChanged);
    connect(m_pLoadFactorSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamCalcDlg::onInputChanged);
    connect(m_pSpanSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamCalcDlg::onInputChanged);
    connect(m_pChordSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamCalcDlg::onInputChanged);
    connect(m_pMaterialCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BeamCalcDlg::onMaterialChanged);
    connect(m_pSafetyFactorSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamCalcDlg::onInputChanged);
    connect(m_pSparWidthSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamCalcDlg::onInputChanged);
    connect(m_pSparHeightSB, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BeamCalcDlg::onInputChanged);

    updateCalculations();
}

void BeamCalcDlg::initMaterials()
{
    m_Materials = {
        {"Aluminum 6061-T6", 69e9, 276e6, 2700},
        {"Carbon/Epoxy UD", 135e9, 600e6, 1600},
        {"Steel 4130", 210e9, 460e6, 7850},
        {"Titanium 6Al-4V", 110e9, 880e6, 4430},
        {"Spruce (Wood)", 10e9, 40e6, 450}
    };
}

void BeamCalcDlg::onInputChanged()
{
    if (m_bBlockSignals) return;
    updateCalculations();
}

void BeamCalcDlg::onMaterialChanged()
{
    onInputChanged();
}

void BeamCalcDlg::updateCalculations()
{
    m_bBlockSignals = true;

    double mass = m_pTotalMassSB->value();
    double n = m_pLoadFactorSB->value();
    double span = m_pSpanSB->value(); // cantilever span
    double chord = m_pChordSB->value();
    double g = 9.81;
    double lift = mass * n * g;

    // Maximum Bending Moment calculation based on Foilinizer logic
    double M_root = 0;
    int distIdx = m_pDistCombo->currentIndex();
    if (distIdx == 0) // Elliptical
    {
        // Correct root moment for elliptical cantilever: M = 4 * L * span / (3 * PI)
        M_root = (4.0 * lift * span) / (3.0 * M_PI);
    }
    else if (distIdx == 1) // Uniform
        M_root = (lift * span) / 2.0;
    else if (distIdx == 2) // Triangular
        M_root = (lift * span) / 3.0;

    m_pMaxMomentLabel->setText(QString("Max Bending Moment: %1 N.m").arg(M_root, 0, 'f', 1));

    int matIdx = m_pMaterialCombo->currentIndex();
    if (matIdx >= 0 && matIdx < static_cast<int>(m_Materials.size()))
    {
        const auto &mat = m_Materials[matIdx];
        double SF = m_pSafetyFactorSB->value();
        double sigma_allow = mat.sigma_y / SF;

        // Required Section Modulus S = M / sigma_allow
        double req_S = M_root / sigma_allow;
        m_pRequiredSLabel->setText(QString("Req. Section Modulus (S): %1 mm³").arg(req_S * 1e9, 0, 'f', 1));

        // Required t/c from Foilinizer logic: t/c = sqrt(S / (k * c^3))
        // Using shape factor k = 0.12 for typical airfoils
        double shape_factor = 0.12;
        double req_tc = std::sqrt(req_S / (shape_factor * std::pow(chord, 3)));
        m_pRequiredTCLabel->setText(QString("Req. Thickness Ratio (t/c): %1 %").arg(req_tc * 100.0, 0, 'f', 2));

        // Box/Cap model: Area = M / (sigma * h)
        double h = m_pSparHeightSB->value() / 1000.0;
        double w = m_pSparWidthSB->value() / 1000.0;
        if (h > 0)
        {
            double A_req = M_root / (sigma_allow * h);
            m_pRequiredAreaLabel->setText(QString("Req. Cap Area (per cap): %1 mm²").arg(A_req * 1e6, 0, 'f', 1));
            
            // Rough weight (spar only)
            double spar_vol = (w * h) * (span / 2.0); // Triangular volume approximation for tapered spar
            m_pEstimatedWeightLabel->setText(QString("Est. Spar Weight (rough): %1 g").arg(spar_vol * mat.rho * 1000, 0, 'f', 0));
        }
    }

    m_bBlockSignals = false;
}