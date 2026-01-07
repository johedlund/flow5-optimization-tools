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

#include <iostream>
#include <vector>
#include <cmath>
#include <QCoreApplication>

#include <api/api.h>
#include <api/foil.h>
#include <api/polar.h>
#include <api/objects2d.h>
#include <interfaces/optim/psotaskfoil.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    std::cout << "Testing Leading Edge Variable Bounds...\n";

    // Create a NACA 2412 foil (cambered, LE at 0,0)
    Foil *pFoil = foil::makeNacaFoil(2412, "test_foil");
    if(!pFoil) {
        std::cerr << "Failed to create foil\n";
        return 1;
    }

    PSOTaskFoil task;
    task.setFoil(pFoil);
    task.setOptimizationPoints(40); // Enough density to have points near LE
    task.setBoundsScale(1.0); 
    
    // We need to force a configuration where Y might be tricky?
    // NACA 2412 LE is at 0,0.
    
    task.initVariablesFromFoil(0.01); // 1% chord delta

    // We need access to internals?
    // PSOTaskFoil does not expose m_Variable directly but we can inspect them via pParticle?
    // No, we can use task.variable(i) from PSOTask base.
    
    // We need to know which variable corresponds to the LE neighbor.
    // PSOTaskFoil::variableBaseIndex(v) gives the node index.
    // We need to find the node index of LE.
    
    int nVars = task.nVariables();
    std::cout << "Generated " << nVars << " variables.\n";

    // Reconstruct base nodes logic to find LE index
    // Note: This duplicates logic from PSOTaskFoil::initVariablesFromFoil somewhat,
    // but we can assume the LE is the node with min X in the foil geometry.
    // Wait, PSOTaskFoil resamples the foil. We can't access m_OptimBaseNodes.
    
    // However, we can infer it. 
    // Variable names are "yb_INDEX".
    // We can parse the index.
    // Or we can assume the variable with base index K is near LE if K is the LE index.
    
    // Let's look at the variable bounds.
    // We expect variables representing Top Surface to have Min >= LE.y
    // We expect variables representing Bottom Surface to have Max <= LE.y
    
    // For NACA 2412, LE.y is 0.0.
    // So Top vars: Min >= 0.0.
    // Bot vars: Max <= 0.0.
    
    bool failed = false;
    
    for(int i=0; i<nVars; ++i)
    {
        const OptVariable& var = task.variable(i);
        int baseIdx = task.variableBaseIndex(i);
        
        // We don't know exactly if it's top or bottom just from index, 
        // but we know the *original* Y value is `task.variableBaseY(i)`.
        
        double originalY = task.variableBaseY(i);
        
        if(originalY > 0.00001) // Clearly Top
        {
            if(var.m_Min < -1.0e-9) {
                std::cout << "FAIL: Top surface variable " << i << " (y=" << originalY << ") has Min < 0: " << var.m_Min << "\n";
                failed = true;
            }
        }
        else if(originalY < -0.00001) // Clearly Bottom
        {
            if(var.m_Max > 1.0e-9) {
                std::cout << "FAIL: Bottom surface variable " << i << " (y=" << originalY << ") has Max > 0: " << var.m_Max << "\n";
                failed = true;
            }
        }
        else 
        {
            // Close to LE. This is the critical zone.
            // If originalY is ~0, it might be the point causing the issue.
            std::cout << "WARN: Variable " << i << " is near LE (y=" << originalY << "). Bounds: [" << var.m_Min << ", " << var.m_Max << "]\n";
            
            // If it's technically "Top" (index < LE), it should be bounded >= 0.
            // But we don't know the index relative to LE here without parsing or internals.
            
            // However, if the bug is "flips down", it implies a Top point was forced below 0.
            // If originalY was e.g. 1e-8, and we forced Max <= 0, that's the bug.
            
            // Let's rely on the visual report: "flips ... to always point down".
            // This implies the bound ALLOWS it to go down, or FORCES it.
        }
    }

    if(failed) return 1;
    
    // Now let's try to verify the "Index vs Y" logic hypothesis.
    // If we have a point with Y=0.0 (exactly) that is technically "Top" (index < LE),
    // and our code said "else { maxY = min(...) }", it forced it to be Bottom.
    // But if it's Top, it should be >= 0.
    
    return 0;
}
