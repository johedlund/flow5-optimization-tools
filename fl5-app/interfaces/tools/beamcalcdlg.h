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
#include <vector>
#include <string>

class QDoubleSpinBox;
class QComboBox;
class QLabel;

class BeamCalcDlg : public QDialog
{
    Q_OBJECT
public:
    explicit BeamCalcDlg(QWidget *pParent = nullptr);

private slots:
    void onInputChanged();
    void onMaterialChanged();

private:
    struct Material {
        std::string name;
        double E; // Young's Modulus (Pa)
        double sigma_y; // Yield Strength (Pa)
        double rho; // Density (kg/m3)
    };

    void initMaterials();
    void updateCalculations();

    QComboBox *m_pDistCombo;

    QDoubleSpinBox *m_pTotalMassSB;
    QDoubleSpinBox *m_pLoadFactorSB;
    QDoubleSpinBox *m_pSpanSB;
    QDoubleSpinBox *m_pChordSB;
    
    QComboBox *m_pMaterialCombo;
    QDoubleSpinBox *m_pSafetyFactorSB;

    // Spar dimensions (box/cap model)
    QDoubleSpinBox *m_pSparWidthSB;
    QDoubleSpinBox *m_pSparHeightSB;

    // Outputs
    QLabel *m_pMaxMomentLabel;
    QLabel *m_pRequiredSLabel;
    QLabel *m_pRequiredAreaLabel;
    QLabel *m_pRequiredTCLabel;
    QLabel *m_pEstimatedWeightLabel;

    std::vector<Material> m_Materials;
    bool m_bBlockSignals = false;
};