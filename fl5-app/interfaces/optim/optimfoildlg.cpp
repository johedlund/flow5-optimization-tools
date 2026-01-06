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

#include <interfaces/optim/optimfoildlg.h>
#include <interfaces/optim/optimizationpanel.h>
#include <api/foil.h>

#include <QVBoxLayout>

OptimFoilDlg::OptimFoilDlg(QWidget *pParent)
    : QDialog(pParent)
{
    setWindowTitle("Optimization 2d (Foil)");
    resize(1200, 750); // Spacious for the split view

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);

    m_pPanel = new OptimizationPanel(this);
    mainLayout->addWidget(m_pPanel);

    connect(m_pPanel, &OptimizationPanel::foilCreated, this, &OptimFoilDlg::foilCreated);
    connect(m_pPanel, &OptimizationPanel::closeRequested, this, &QDialog::accept);
}

OptimFoilDlg::~OptimFoilDlg()
{
}

void OptimFoilDlg::initDialog(Foil *pFoil, Polar *pPolar)
{
    m_pPanel->initPanel(pFoil, pPolar);
}
