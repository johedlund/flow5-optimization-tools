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

#include <interfaces/optim/psotaskfoil.h>

#include <api/foil.h>
#include <api/oppoint.h>
#include <api/polar.h>
#include <api/xfoiltask.h>

#include <algorithm>
#include <cmath>
#include <string>

PSOTaskFoil::PSOTaskFoil()
{
}

PSOTaskFoil::~PSOTaskFoil()
{
    if(m_pFoil) delete m_pFoil;
    if(m_pPolar) delete m_pPolar;
}

void PSOTaskFoil::setFoil(Foil *pFoil)
{
    if(m_pFoil) delete m_pFoil;
    if(pFoil)
        m_pFoil = new Foil(pFoil);
    else
        m_pFoil = nullptr;
}

void PSOTaskFoil::setPolar(Polar *pPolar)
{
    if(m_pPolar) delete m_pPolar;
    if(pPolar)
        m_pPolar = new Polar(*pPolar);
    else
        m_pPolar = nullptr;
}

int PSOTaskFoil::variableBaseIndex(int varIndex) const
{
    if(varIndex < 0 || varIndex >= int(m_VarToBase.size()))
        return -1;
    return m_VarToBase[varIndex];
}

double PSOTaskFoil::variableBaseY(int varIndex) const
{
    const int baseIndex = variableBaseIndex(varIndex);
    if(baseIndex < 0 || baseIndex >= int(m_OptimBaseNodes.size()))
        return 0.0;
    return m_OptimBaseNodes[baseIndex].y;
}

Foil* PSOTaskFoil::createOptimizedFoil(const Particle &p) const
{
    if(!m_pFoil || p.dimension() != nVariables())
        return nullptr;

    Foil *pNewFoil = new Foil(m_pFoil);

    if(m_Preset == PresetType::V2_Camber_Thickness)
    {
        if(p.dimension() < 4) { delete pNewFoil; return nullptr; }

        double maxC = p.pos(0);
        double maxT = p.pos(1);
        double xC   = p.pos(2);
        double xT   = p.pos(3);

        pNewFoil->setCamber(xC, maxC);
        pNewFoil->setThickness(xT, maxT);
        pNewFoil->makeBaseFromCamberAndThickness();
        pNewFoil->rebuildPointSequenceFromBase();
    }
    else // V1
    {
        if(!m_OptimBaseNodes.empty())
            pNewFoil->setBaseNodes(m_OptimBaseNodes);

        for(int i=0; i<int(m_VarToBase.size()); ++i)
        {
            const int baseIndex = m_VarToBase[i];
            pNewFoil->setBaseNode(baseIndex, pNewFoil->xb(baseIndex), p.pos(i));
        }
    }

    if(!pNewFoil->initGeometry())
    {
        delete pNewFoil;
        return nullptr;
    }

    // Resample back to high resolution if optimization was done on low-res
    if (m_OptimizationPoints > 0 && m_pFoil->nBaseNodes() > m_OptimizationPoints)
    {
        pNewFoil->rePanel(m_pFoil->nBaseNodes(), 1.0);
    }

    pNewFoil->setName(m_pFoil->name() + "_optimized");
    return pNewFoil;
}

void PSOTaskFoil::setTargetAlpha(double alpha)
{
    m_TargetMode = TargetMode::Alpha;
    m_TargetValue = alpha;
}

void PSOTaskFoil::setTargetCl(double cl)
{
    m_TargetMode = TargetMode::Cl;
    m_TargetValue = cl;
}

bool PSOTaskFoil::resolveTarget(bool &useAlpha, double &value) const
{
    if(!m_pPolar)
        return false;

    if(m_TargetMode == TargetMode::Alpha)
    {
        useAlpha = true;
        value = m_TargetValue;
        return std::isfinite(value);
    }

    if(m_TargetMode == TargetMode::Cl)
    {
        useAlpha = false;
        value = m_TargetValue;
        return std::isfinite(value);
    }

    if(m_pPolar->isFixedLiftPolar())
    {
        useAlpha = false;
        if(!m_pPolar->m_Cl.empty())
        {
            value = m_pPolar->m_Cl.front();
            return std::isfinite(value);
        }
        return false;
    }

    useAlpha = true;
    if(m_pPolar->isFixedaoaPolar())
    {
        value = m_pPolar->aoaSpec();
        return std::isfinite(value);
    }

    if(!m_pPolar->m_Alpha.empty())
    {
        value = m_pPolar->m_Alpha.front();
        return std::isfinite(value);
    }

    return false;
}

void PSOTaskFoil::initVariablesFromFoil(double yDelta)
{
    m_Variable.clear();
    m_VarToBase.clear();
    m_OptimBaseNodes.clear();
    m_OptimBaseIndex.clear();

    if(!m_pFoil)
        return;

    if(m_Preset == PresetType::V2_Camber_Thickness)
    {
        m_BaseMaxCamber = m_pFoil->maxCamber();
        m_BaseMaxThickness = m_pFoil->maxThickness();
        m_BaseXCamber = m_pFoil->xCamber();
        m_BaseXThickness = m_pFoil->xThickness();

        // 1. Max Camber
        double delta = std::max(0.005, std::fabs(m_BaseMaxCamber) * 0.5);
        m_Variable.emplace_back("MaxCamber", m_BaseMaxCamber - delta, m_BaseMaxCamber + delta);

        // 2. Max Thickness
        delta = std::max(0.005, std::fabs(m_BaseMaxThickness) * 0.5);
        m_Variable.emplace_back("MaxThickness", std::max(0.01, m_BaseMaxThickness - delta), m_BaseMaxThickness + delta);

        // 3. Max Camber Position
        delta = 0.1; 
        m_Variable.emplace_back("XCamber", std::max(0.1, m_BaseXCamber - delta), std::min(0.9, m_BaseXCamber + delta));

        // 4. Max Thickness Position
        delta = 0.1;
        m_Variable.emplace_back("XThickness", std::max(0.1, m_BaseXThickness - delta), std::min(0.9, m_BaseXThickness + delta));
        return; 
    }

    const int nBase = m_pFoil->nBaseNodes();
    if(nBase <= 0)
        return;

    // Preset V1: Y-only variation of base nodes
    // Constraints: LE and TE points are fixed
    // Use a capped base node set to keep spline solves lightweight.

    int leIndex = m_pFoil->LEindex();
    if(leIndex < 0 || leIndex >= nBase)
    {
        double minX = m_pFoil->xb(0);
        leIndex = 0;
        for(int i=1; i<nBase; ++i)
        {
            if(m_pFoil->xb(i) < minX)
            {
                minX = m_pFoil->xb(i);
                leIndex = i;
            }
        }
    }

    const int maxBaseNodes = (m_OptimizationPoints > 0) ? m_OptimizationPoints : 80;
    std::vector<int> baseIndices;
    baseIndices.reserve(nBase);

    if(nBase <= maxBaseNodes)
    {
        for(int i=0; i<nBase; ++i)
            baseIndices.push_back(i);
    }
    else
    {
        const int nCtrl = std::max(3, maxBaseNodes);
        const double step = double(nBase-1) / double(nCtrl-1);
        for(int i=0; i<nCtrl-1; ++i)
        {
            const int idx = int(std::round(double(i) * step));
            baseIndices.push_back(idx);
        }
        baseIndices.push_back(nBase-1);
    }

    baseIndices.push_back(0);
    baseIndices.push_back(nBase-1);
    baseIndices.push_back(leIndex);
    if(leIndex > 0)
        baseIndices.push_back(leIndex - 1);
    if(leIndex + 1 < nBase)
        baseIndices.push_back(leIndex + 1);

    std::sort(baseIndices.begin(), baseIndices.end());
    baseIndices.erase(std::unique(baseIndices.begin(), baseIndices.end()), baseIndices.end());

    m_OptimBaseNodes.reserve(baseIndices.size());
    m_OptimBaseIndex.reserve(baseIndices.size());
    for(int idx : baseIndices)
    {
        m_OptimBaseNodes.emplace_back(m_pFoil->xb(idx), m_pFoil->yb(idx));
        m_OptimBaseIndex.push_back(idx);
    }

    if(m_OptimBaseNodes.empty())
        return;

    int leOptimIndex = 0;
    double minX = m_OptimBaseNodes.front().x;
    for(int i=1; i<int(m_OptimBaseNodes.size()); ++i)
    {
        if(m_OptimBaseNodes[i].x < minX)
        {
            minX = m_OptimBaseNodes[i].x;
            leOptimIndex = i;
        }
    }

    const double maxThickness = std::fabs(m_pFoil->maxThickness());
    double delta = (yDelta > 0.0) ? yDelta : std::max(0.002, 0.2 * maxThickness);
    delta *= m_BoundsScale;

    const int nOptim = int(m_OptimBaseNodes.size());
    m_Variable.reserve(nOptim);
    const double yLE = m_OptimBaseNodes[leOptimIndex].y;

    for(int i=0; i<nOptim; ++i)
    {
        // Freeze LE and immediate neighbors to preserve nose curvature and prevent artifacts
        if(i == 0 || i == nOptim-1 || 
           i == leOptimIndex || 
           i == leOptimIndex - 1 || 
           i == leOptimIndex + 1)
            continue;

        const double y = m_OptimBaseNodes[i].y;
        double minY = y - delta;
        double maxY = y + delta;

        // Constraint: Prevent crossover at Leading Edge level for remaining points
        // This prevents points further back from crossing the nose line and causing tangles
        if (y >= yLE) {
            minY = std::max(minY, yLE + 1.0e-6);
        } else {
            maxY = std::min(maxY, yLE - 1.0e-6);
        }
        
        m_Variable.emplace_back("yb_" + std::to_string(m_OptimBaseIndex[i]), minY, maxY);
        m_VarToBase.push_back(i);
    }

    if(m_VarToBase.empty() && nOptim > 2)
    {
        int mid = nOptim/2;
        if(mid == 0 || mid == nOptim-1 || mid == leOptimIndex)
            mid = (mid+1 < nOptim-1) ? mid+1 : 1;

        const double y = m_OptimBaseNodes[mid].y;
        m_Variable.emplace_back("yb_" + std::to_string(m_OptimBaseIndex[mid]), y - delta, y + delta);
        m_VarToBase.push_back(mid);
    }
}

void PSOTaskFoil::calcFitness(Particle *pParticle, bool bLong, bool bTrace) const
{
    (void)bLong;

    if(!pParticle)
        return;

    for(int i=0; i<pParticle->nObjectives(); ++i)
    {
        pParticle->setFitness(i, OPTIM_PENALTY);
        pParticle->setError(i, OPTIM_PENALTY);
    }

    pParticle->setConverged(false);

    if(!m_pFoil)
        return;

    if(pParticle->dimension() != nVariables())
        return;

    Foil workFoil(m_pFoil);

    if(m_Preset == PresetType::V2_Camber_Thickness)
    {
        if(pParticle->dimension() < 4) return;

        double maxC = pParticle->pos(0);
        double maxT = pParticle->pos(1);
        double xC   = pParticle->pos(2);
        double xT   = pParticle->pos(3);

        workFoil.setCamber(xC, maxC);
        workFoil.setThickness(xT, maxT);
        workFoil.makeBaseFromCamberAndThickness();
        workFoil.rebuildPointSequenceFromBase();
    }
    else // V1
    {
        if(!m_OptimBaseNodes.empty())
            workFoil.setBaseNodes(m_OptimBaseNodes);

        for(int i=0; i<int(m_VarToBase.size()); ++i)
        {
            const int baseIndex = m_VarToBase[i];
            workFoil.setBaseNode(baseIndex, workFoil.xb(baseIndex), pParticle->pos(i));
        }
    }

    if(!workFoil.initGeometry())
        return;

    if (m_Constraints.enabled)
    {
        double penalty = 0.0;

        if (m_Constraints.minThickness.enabled && m_Constraints.minThickness.value > 0.0) {
            double t = workFoil.maxThickness();
            if (t < m_Constraints.minThickness.value)
                penalty += std::pow(m_Constraints.minThickness.value - t, 2) * 1000.0;
        }

        if (m_Constraints.maxThickness.enabled && m_Constraints.maxThickness.value > 0.0 && m_Constraints.maxThickness.value < 1.0) {
            double t = workFoil.maxThickness();
            if (t > m_Constraints.maxThickness.value)
                penalty += std::pow(t - m_Constraints.maxThickness.value, 2) * 1000.0;
        }

        if (m_Constraints.minTEThickness.enabled && m_Constraints.minTEThickness.value > 0.0) {
            double te = workFoil.TEGap();
            if (te < m_Constraints.minTEThickness.value)
                penalty += std::pow(m_Constraints.minTEThickness.value - te, 2) * 5000.0;
        }

        if (m_Constraints.minLERadius.enabled && m_Constraints.minLERadius.value > 0.0) {
            double r = workFoil.LERadius();
            if (r < m_Constraints.minLERadius.value)
                penalty += std::pow(m_Constraints.minLERadius.value - r, 2) * 50000.0;
        }

        if (m_Constraints.maxWiggliness.enabled && m_Constraints.maxWiggliness.value > 0.0) {
            double w = workFoil.wiggliness();
            if (w > m_Constraints.maxWiggliness.value)
                penalty += (w - m_Constraints.maxWiggliness.value) * 1.0;
        }

        if (m_Constraints.minSectionModulus.enabled && m_Constraints.minSectionModulus.value > 0.0) {
            double t = workFoil.maxThickness();
            double s = 0.12 * t * t; // approx S/c^3
            if (s < m_Constraints.minSectionModulus.value)
                penalty += std::pow(m_Constraints.minSectionModulus.value - s, 2) * 100000.0;
        }

        // New Geometric Constraints
        if (m_Constraints.minCamber.enabled) {
            double c = workFoil.maxCamber();
            if (c < m_Constraints.minCamber.value)
                penalty += std::pow(m_Constraints.minCamber.value - c, 2) * 1000.0;
        }
        if (m_Constraints.maxCamber.enabled) {
            double c = workFoil.maxCamber();
            if (c > m_Constraints.maxCamber.value)
                penalty += std::pow(c - m_Constraints.maxCamber.value, 2) * 1000.0;
        }

        if (m_Constraints.minXCamber.enabled) {
            double xc = workFoil.xCamber();
            if (xc < m_Constraints.minXCamber.value)
                penalty += std::pow(m_Constraints.minXCamber.value - xc, 2) * 1000.0;
        }
        if (m_Constraints.maxXCamber.enabled) {
            double xc = workFoil.xCamber();
            if (xc > m_Constraints.maxXCamber.value)
                penalty += std::pow(xc - m_Constraints.maxXCamber.value, 2) * 1000.0;
        }

        if (m_Constraints.minXThickness.enabled) {
            double xt = workFoil.xThickness();
            if (xt < m_Constraints.minXThickness.value)
                penalty += std::pow(m_Constraints.minXThickness.value - xt, 2) * 1000.0;
        }
        if (m_Constraints.maxXThickness.enabled) {
            double xt = workFoil.xThickness();
            if (xt > m_Constraints.maxXThickness.value)
                penalty += std::pow(xt - m_Constraints.maxXThickness.value, 2) * 1000.0;
        }

        if (m_Constraints.minArea.enabled) {
            // Approximate area or use precise calculation if available
            // For now, let's assume we can compute it from the polygon
            double area = 0.0;
            const int n = workFoil.nBaseNodes(); // Using base nodes for speed
            for(int i=0; i<n-1; ++i) {
                area += (workFoil.xb(i) * workFoil.yb(i+1) - workFoil.xb(i+1) * workFoil.yb(i));
            }
            area = std::fabs(0.5 * area); // Polygon area
            
            if (area < m_Constraints.minArea.value)
                penalty += std::pow(m_Constraints.minArea.value - area, 2) * 10000.0;
        }

        if (penalty > 0.0) {
            // Apply penalty to all objectives
            for(int i=0; i<pParticle->nObjectives(); ++i)
                pParticle->setFitness(i, OPTIM_PENALTY + penalty);
            return;
        }
    }

    Polar workPolar;
    if(m_pPolar)
        workPolar.copySpecification(*m_pPolar);
    
    // Override with task settings
    workPolar.setReynolds(m_Reynolds);
    workPolar.setMach(m_Mach);
    workPolar.setNCrit(m_NCrit);

    workPolar.reset();
    workPolar.setFoilName(workFoil.name());

    // Allocate XFoilTask on heap to prevent stack overflow (XFoil has large arrays)
    // and ensure thread safety regarding stack size limits.
    XFoilTask *task = new XFoilTask();
    XFoilTask::setCancelled(false);

    bool useAlpha = true;
    double target = 0.0;
    if(!resolveTarget(useAlpha, target)) {
        delete task;
        return;
    }

    task->setAoAAnalysis(useAlpha);
    task->clearRanges();
    task->appendRange({true, target, target, 0.0});

    if(!task->initialize(workFoil, &workPolar, true)) {
        delete task;
        return;
    }

    task->run();

    const std::vector<OpPoint*> &opps = task->operatingPoints();
    if(!opps.empty() && !task->hasErrors())
    {
        const OpPoint *pOpp = opps.front();
        const double cl = pOpp->m_Cl;
        const double cd = pOpp->m_Cd;
        const double cm = pOpp->m_Cm;

        // Robustness check: Ensure values are finite
        if (!std::isfinite(cl) || !std::isfinite(cd) || !std::isfinite(cm))
            return; // Leave as penalty

        // Check Aerodynamic Constraints
        double aeroPenalty = 0.0;
        if (m_Constraints.enabled)
        {
            if (m_Constraints.minCl.enabled && cl < m_Constraints.minCl.value)
                aeroPenalty += std::pow(m_Constraints.minCl.value - cl, 2) * 1000.0;
            if (m_Constraints.maxCl.enabled && cl > m_Constraints.maxCl.value)
                aeroPenalty += std::pow(cl - m_Constraints.maxCl.value, 2) * 1000.0;

            if (m_Constraints.minCd.enabled && cd < m_Constraints.minCd.value)
                aeroPenalty += std::pow(m_Constraints.minCd.value - cd, 2) * 10000.0;
            if (m_Constraints.maxCd.enabled && cd > m_Constraints.maxCd.value)
                aeroPenalty += std::pow(cd - m_Constraints.maxCd.value, 2) * 10000.0;

            if (m_Constraints.minCm.enabled && cm < m_Constraints.minCm.value)
                aeroPenalty += std::pow(m_Constraints.minCm.value - cm, 2) * 1000.0;
            if (m_Constraints.maxCm.enabled && cm > m_Constraints.maxCm.value)
                aeroPenalty += std::pow(cm - m_Constraints.maxCm.value, 2) * 1000.0;

            double ld = (std::abs(cd) > 1.0e-9) ? cl/cd : 0.0;
            if (m_Constraints.minLD.enabled && ld < m_Constraints.minLD.value)
                aeroPenalty += std::pow(m_Constraints.minLD.value - ld, 2) * 10.0;
        }

        if (aeroPenalty > 0.0)
        {
             for(int iobj=0; iobj<pParticle->nObjectives(); ++iobj)
                pParticle->setFitness(iobj, OPTIM_PENALTY + aeroPenalty);
             // Cleanup
             for(OpPoint *pOpp : opps) delete pOpp;
             delete task;
             return;
        }

        for(int iobj=0; iobj<pParticle->nObjectives(); ++iobj)
        {
            OptObjective const &obj = m_Objective.at(iobj);
            double val = OPTIM_PENALTY;

            switch(m_ObjectiveType)
            {
                case ObjectiveType::MinimizeCd:
                    val = cd;
                    break;
                case ObjectiveType::MaximizeLD:
                    val = (cd > 1.0e-9) ? cl/cd : -OPTIM_PENALTY; // Return -penalty if invalid L/D
                    break;
                case ObjectiveType::MaximizeCl:
                    val = -cl; // Minimizer expects lower is better, so negate maximization targets
                    break;
                case ObjectiveType::MinimizeCm:
                    val = std::abs(cm);
                    break;
                case ObjectiveType::TargetCl:
                    val = std::abs(cl - m_TargetValue);
                    break;
                case ObjectiveType::TargetCm:
                    val = std::abs(cm - m_TargetValue);
                    break;
                case ObjectiveType::MaximizePowerFactor: // Cl^1.5 / Cd
                    if (cl > 0.0 && cd > 1.0e-9)
                        val = -std::pow(cl, 1.5) / cd;
                    else
                        val = OPTIM_PENALTY;
                    break;
                case ObjectiveType::MaximizeEnduranceFactor: // Cl^3 / Cd^2
                     if (cl > 0.0 && cd > 1.0e-9)
                        val = -std::pow(cl, 3.0) / std::pow(cd, 2.0);
                    else
                        val = OPTIM_PENALTY;
                    break;
            }
            
            if (std::isfinite(val))
                 pParticle->setFitness(iobj, val);
            else
                 pParticle->setFitness(iobj, OPTIM_PENALTY);
        }
        pParticle->setConverged(true);
    }

    for(OpPoint *pOpp : opps)
        delete pOpp;

    delete task;

    if(bTrace) postParticleEvent();
}
