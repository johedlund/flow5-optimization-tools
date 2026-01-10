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
#include <geom_global.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace {

bool isMonotonicXAboutLE(const std::vector<Node2d> &pts, double eps)
{
    const int n = static_cast<int>(pts.size());
    if(n < 3)
        return true;

    int leIndex = 0;
    double minX = pts[0].x;
    for(int i=1; i<n; ++i)
    {
        if(pts[i].x < minX)
        {
            minX = pts[i].x;
            leIndex = i;
        }
    }

    for(int i=1; i<=leIndex; ++i)
    {
        if(pts[i].x > pts[i-1].x + eps)
            return false;
    }

    for(int i=leIndex+1; i<n; ++i)
    {
        if(pts[i].x + eps < pts[i-1].x)
            return false;
    }

    return true;
}

double curvatureFromPoints(const Node2d &a, const Node2d &b, const Node2d &c)
{
    const double abx = b.x - a.x;
    const double aby = b.y - a.y;
    const double bcx = c.x - b.x;
    const double bcy = c.y - b.y;
    const double acx = c.x - a.x;
    const double acy = c.y - a.y;

    const double lab = std::sqrt(abx * abx + aby * aby);
    const double lbc = std::sqrt(bcx * bcx + bcy * bcy);
    const double lac = std::sqrt(acx * acx + acy * acy);

    if(lab < 1.0e-9 || lbc < 1.0e-9 || lac < 1.0e-9)
        return 0.0;

    const double cross = std::fabs(abx * acy - aby * acx);
    return 2.0 * cross / (lab * lbc * lac);
}

double maxCurvatureNearLE(const std::vector<Node2d> &pts, double leLimitX)
{
    double maxCurv = 0.0;
    const int n = static_cast<int>(pts.size());
    for(int i=1; i<n-1; ++i)
    {
        if(pts[i].x <= leLimitX)
        {
            const double curv = curvatureFromPoints(pts[i-1], pts[i], pts[i+1]);
            if(curv > maxCurv)
                maxCurv = curv;
        }
    }
    return maxCurv;
}

double maxTurnAngleNearLE(const std::vector<Node2d> &pts, double leLimitX)
{
    double maxAngle = 0.0;
    const int n = static_cast<int>(pts.size());
    for(int i=1; i<n-1; ++i)
    {
        if(pts[i].x <= leLimitX)
        {
            const double v0x = pts[i].x - pts[i-1].x;
            const double v0y = pts[i].y - pts[i-1].y;
            const double v1x = pts[i+1].x - pts[i].x;
            const double v1y = pts[i+1].y - pts[i].y;
            const double l0 = std::sqrt(v0x * v0x + v0y * v0y);
            const double l1 = std::sqrt(v1x * v1x + v1y * v1y);
            if(l0 < 1.0e-9 || l1 < 1.0e-9)
                continue;
            double dot = (v0x * v1x + v0y * v1y) / (l0 * l1);
            dot = std::max(-1.0, std::min(1.0, dot));
            const double angle = std::acos(dot);
            if(angle > maxAngle)
                maxAngle = angle;
        }
    }
    return maxAngle;
}

bool interpolateY(const std::vector<Node2d> &curve, double x, double &yOut)
{
    const int n = static_cast<int>(curve.size());
    if(n < 2)
        return false;

    for(int i=0; i<n-1; ++i)
    {
        const double x0 = curve[i].x;
        const double x1 = curve[i+1].x;
        if((x0 <= x && x <= x1) || (x1 <= x && x <= x0))
        {
            const double dx = x1 - x0;
            if(std::fabs(dx) < 1.0e-12)
            {
                yOut = curve[i].y;
                return true;
            }
            const double t = (x - x0) / dx;
            yOut = curve[i].y + t * (curve[i+1].y - curve[i].y);
            return true;
        }
    }
    return false;
}

bool hasPositiveThicknessNearLE(const std::vector<Node2d> &pts, double minX, double chord)
{
    const int n = static_cast<int>(pts.size());
    if(n < 4 || chord <= 0.0)
        return true;

    int leIndex = 0;
    double leX = pts[0].x;
    for(int i=1; i<n; ++i)
    {
        if(pts[i].x < leX)
        {
            leX = pts[i].x;
            leIndex = i;
        }
    }

    std::vector<Node2d> top;
    std::vector<Node2d> bot;
    top.reserve(leIndex + 1);
    bot.reserve(n - leIndex);

    for(int i=leIndex; i>=0; --i)
        top.push_back(pts[i]);
    for(int i=leIndex; i<n; ++i)
        bot.push_back(pts[i]);

    const double eps = chord * 1.0e-5;
    const double x0 = minX + chord * 0.005;
    const double x1 = minX + chord * 0.05;
    const int samples = 5;

    for(int i=0; i<samples; ++i)
    {
        const double t = (samples == 1) ? 0.0 : double(i) / double(samples - 1);
        const double x = x0 + (x1 - x0) * t;
        double yTop = 0.0;
        double yBot = 0.0;
        if(!interpolateY(top, x, yTop) || !interpolateY(bot, x, yBot))
            continue;
        if(yTop <= yBot + eps)
            return false;
    }

    return true;
}

bool hasSelfIntersection(const std::vector<Node2d> &pts)
{
    const int n = static_cast<int>(pts.size());
    if(n < 4)
        return false;

    Vector2d ip;
    constexpr double precision = 1.0e-7;
    for(int i=0; i<n-1; ++i)
    {
        for(int j=i+2; j<n-1; ++j)
        {
            if(geom::intersectSegment(pts[i], pts[i+1], pts[j], pts[j+1], ip, false, precision))
                return true;
        }
    }
    return false;
}

struct LeMetrics
{
    bool hasMonotonic{false};
    bool hasPositiveThickness{false};
    bool hasSelfIntersection{false};
    double leRadius{0.0};
    double maxCurvature{0.0};
    double maxTurnAngle{0.0};
};

bool computeLeMetrics(const Foil &foil, LeMetrics &metrics)
{
    metrics = {};
    metrics.leRadius = foil.LERadius();

    const std::vector<Node2d> &pts = foil.cubicSpline().outputPts();
    if(pts.size() < 4)
        return false;

    double minX = pts.front().x;
    double maxX = pts.front().x;
    for(const auto &pt : pts)
    {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
    }
    const double chord = maxX - minX;
    if(chord <= 0.0)
        return false;

    const double eps = chord * 1.0e-4;
    metrics.hasMonotonic = isMonotonicXAboutLE(pts, eps);
    metrics.hasPositiveThickness = hasPositiveThicknessNearLE(pts, minX, chord);
    metrics.hasSelfIntersection = hasSelfIntersection(pts);
    metrics.maxCurvature = maxCurvatureNearLE(pts, minX + chord * 0.06);
    metrics.maxTurnAngle = maxTurnAngleNearLE(pts, minX + chord * 0.06);
    return true;
}

bool isFoilGeometryValid(const Foil &foil,
                         double baseLERadius,
                         double baseMaxLECurv,
                         double baseMaxLETurnAngle,
                         bool baseHasMonotonicLE,
                         bool baseHasPositiveThicknessLE,
                         bool baseHasSelfIntersection)
{
    if(foil.TEGap() < 0.0)
        return false;

    const std::vector<Node2d> &pts = foil.cubicSpline().outputPts();
    if(pts.size() < 4)
        return true;

    double minX = pts.front().x;
    double maxX = pts.front().x;
    for(const auto &pt : pts)
    {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
    }
    const double chord = maxX - minX;
    if(chord <= 0.0)
        return false;

    const double eps = chord * 1.0e-4;
    if(baseHasMonotonicLE && !isMonotonicXAboutLE(pts, eps))
        return false;

    if(!baseHasSelfIntersection && hasSelfIntersection(pts))
        return false;

    if(baseHasPositiveThicknessLE && !hasPositiveThicknessNearLE(pts, minX, chord))
        return false;

    if(std::isfinite(baseLERadius) && baseLERadius > 0.0)
    {
        const double minRadius = std::max(baseLERadius * 0.25, chord * 1.0e-4);
        if(foil.LERadius() < minRadius)
            return false;
    }

    if(std::isfinite(baseMaxLECurv) && baseMaxLECurv > 0.0)
    {
        const double leLimitX = minX + chord * 0.06;
        const double maxCurv = maxCurvatureNearLE(pts, leLimitX);
        if(maxCurv > baseMaxLECurv * 2.5)
            return false;
    }

    if(std::isfinite(baseMaxLETurnAngle) && baseMaxLETurnAngle > 0.0)
    {
        const double leLimitX = minX + chord * 0.06;
        const double maxAngle = maxTurnAngleNearLE(pts, leLimitX);
        const double maxAllowed = std::max(baseMaxLETurnAngle * 1.6, baseMaxLETurnAngle + 0.4);
        if(maxAngle > maxAllowed)
            return false;
    }

    return true;
}

// Calculate dCp/dx (pressure gradient) on the upper surface at a specific x/c position
// Returns the gradient value, or NaN if calculation fails
// Positive dCp/dx indicates adverse pressure gradient (pressure recovery)
double upperSurfaceDCpDx(const Foil &foil, const OpPoint &opp, double xTarget)
{
    const int nPts = opp.nPoints();
    if(nPts < 4)
        return std::nan("");

    // Find LE index (minimum x)
    int leIndex = 0;
    double minX = foil.xb(0);
    for(int i = 1; i < nPts && i < foil.nBaseNodes(); ++i)
    {
        if(foil.xb(i) < minX)
        {
            minX = foil.xb(i);
            leIndex = i;
        }
    }

    // Upper surface goes from index 0 (TE) to leIndex (LE)
    // x decreases from ~1 to ~0 on upper surface
    // We need to find two consecutive points bracketing xTarget

    // Search for the segment containing xTarget on upper surface
    // Points are ordered: TE(x≈1) → LE(x≈0)
    for(int i = 0; i < leIndex && i < nPts - 1; ++i)
    {
        const double x0 = foil.xb(i);
        const double x1 = foil.xb(i + 1);

        // Check if xTarget is between x0 and x1 (note: x0 > x1 on upper surface)
        if((x0 >= xTarget && xTarget >= x1) || (x1 >= xTarget && xTarget >= x0))
        {
            const double dx = x0 - x1;
            if(std::fabs(dx) < 1.0e-10)
                continue;

            const double cp0 = opp.m_Cpv[i];
            const double cp1 = opp.m_Cpv[i + 1];
            const double dCp = cp1 - cp0;

            // dCp/dx: positive means Cp increasing with x (favorable on upper surface near LE)
            // We want to detect adverse gradients where Cp increases as we go toward TE
            // On upper surface going from LE to TE: x increases, Cp should increase (adverse)
            // Return dCp/dx in the direction of increasing x (toward TE)
            return dCp / dx;
        }
    }

    return std::nan("");
}

} // namespace

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

bool PSOTaskFoil::variableIsX(int varIndex) const
{
    if(varIndex < 0 || varIndex >= int(m_VarIsX.size()))
        return false;
    return m_VarIsX[varIndex];
}

double PSOTaskFoil::variableBaseValue(int varIndex) const
{
    const int baseIndex = variableBaseIndex(varIndex);
    if(baseIndex < 0 || baseIndex >= int(m_OptimBaseNodes.size()))
        return 0.0;
    if(variableIsX(varIndex))
        return m_OptimBaseNodes[baseIndex].x;
    return m_OptimBaseNodes[baseIndex].y;
}

Foil* PSOTaskFoil::createOptimizedFoil(const Particle &p) const
{
    if(!m_pFoil || p.dimension() != nVariables())
        return nullptr;

    Foil *pNewFoil = new Foil(m_pFoil);

    if(m_Preset == PresetType::V2_Camber_Thickness)
    {
        double maxC, maxT, xC, xT;

        if(m_bSymmetric)
        {
            // Symmetric: only [MaxThickness, XThickness] variables, camber forced to 0
            if(p.dimension() < 2) { delete pNewFoil; return nullptr; }
            maxC = 0.0;
            maxT = p.pos(0);
            xC = 0.3; // Fixed camber position (doesn't matter since camber = 0)
            xT = p.pos(1);
        }
        else
        {
            if(p.dimension() < 4) { delete pNewFoil; return nullptr; }
            maxC = p.pos(0);
            maxT = p.pos(1);
            xC   = p.pos(2);
            xT   = p.pos(3);
        }

        pNewFoil->setCamber(xC, maxC);
        pNewFoil->setThickness(xT, maxT);
        pNewFoil->makeBaseFromCamberAndThickness();
        pNewFoil->rebuildPointSequenceFromBase();
    }
    else if(m_Preset == PresetType::V3_BSpline_Control)
    {
        // V3: Create foil from modified B-spline control points
        const int nCtrl = m_BaseBSpline.nCtrlPoints();
        if(nCtrl < 4) { delete pNewFoil; return nullptr; }

        // Copy the base B-spline and modify control point Y coordinates
        BSpline workBSpline;
        workBSpline.duplicate(m_BaseBSpline);

        // Apply particle position values to control points (X or Y depending on m_VarIsX)
        for(int i = 0; i < int(m_VarToBase.size()); ++i)
        {
            const int ctrlIndex = m_VarToBase[i];
            Node2d pt = workBSpline.controlPoint(ctrlIndex);
            if(i < int(m_VarIsX.size()) && m_VarIsX[i])
                pt.x = p.pos(i);
            else
                pt.y = p.pos(i);
            workBSpline.setCtrlPoint(ctrlIndex, pt);
        }

        // Generate the output curve from modified control points
        workBSpline.updateSpline();
        workBSpline.makeCurve();

        const std::vector<Node2d> &output = workBSpline.outputPts();
        if(output.size() < 10) { delete pNewFoil; return nullptr; }

        // For symmetric mode, mirror at output curve level (not control points)
        if(m_bSymmetric)
        {
            // Find LE index in output curve (minimum X)
            int leIdx = 0;
            double minX = output[0].x;
            for(int i = 1; i < int(output.size()); ++i)
            {
                if(output[i].x < minX)
                {
                    minX = output[i].x;
                    leIdx = i;
                }
            }

            // Build symmetric foil: upper surface mirrored to lower
            // Output is ordered: TE(upper) -> LE -> TE(lower)
            std::vector<Node2d> symPts;
            symPts.reserve(output.size());

            // Copy upper surface (0 to leIdx)
            for(int i = 0; i <= leIdx; ++i)
                symPts.push_back(output[i]);

            // Set LE to y=0
            symPts[leIdx].y = 0.0;

            // Mirror upper surface to create lower (skip LE, go from leIdx-1 to 0)
            for(int i = leIdx - 1; i >= 0; --i)
            {
                Node2d pt = symPts[i];
                pt.y = -pt.y;
                symPts.push_back(pt);
            }

            pNewFoil->setBaseNodes(symPts);
        }
        else
        {
            pNewFoil->setBaseNodes(output);
        }
    }
    else // V1
    {
        if(!m_OptimBaseNodes.empty())
            pNewFoil->setBaseNodes(m_OptimBaseNodes);

        // Find LE index and chord in the optimized base nodes
        int leOptimIndex = 0;
        const int nOptim = int(m_OptimBaseNodes.size());
        double minX = m_OptimBaseNodes.empty() ? 0.0 : m_OptimBaseNodes[0].x;
        double maxX = minX;
        for(int i = 1; i < nOptim; ++i)
        {
            if(m_OptimBaseNodes[i].x < minX)
            {
                minX = m_OptimBaseNodes[i].x;
                leOptimIndex = i;
            }
            if(m_OptimBaseNodes[i].x > maxX)
                maxX = m_OptimBaseNodes[i].x;
        }
        const double chord = maxX - minX;

        // Apply particle position values (X or Y depending on m_VarIsX)
        for(int i=0; i<int(m_VarToBase.size()); ++i)
        {
            const int baseIndex = m_VarToBase[i];
            if(i < int(m_VarIsX.size()) && m_VarIsX[i])
                pNewFoil->setBaseNode(baseIndex, p.pos(i), pNewFoil->yb(baseIndex));
            else
                pNewFoil->setBaseNode(baseIndex, pNewFoil->xb(baseIndex), p.pos(i));
        }

        // LE blend: smoothly transition from original shape near LE to optimized shape
        // This prevents spline oscillations from distorting the LE region
        if(m_LEBlendChord > 0.0 && chord > 0.0)
        {
            const double leBlendEnd = minX + m_LEBlendChord * chord;
            for(int i = 0; i < nOptim; ++i)
            {
                if(i == leOptimIndex)
                    continue;  // Skip LE point itself

                const double x = pNewFoil->xb(i);
                if(x < leBlendEnd)
                {
                    // Calculate blend factor: 0 at LE, 1 at blend end (smoothstep)
                    const double t = (x - minX) / (leBlendEnd - minX);
                    const double blend = t * t * (3.0 - 2.0 * t);  // smoothstep

                    // Blend Y between original and optimized
                    const double originalY = m_OptimBaseNodes[i].y;
                    const double optimizedY = pNewFoil->yb(i);
                    const double blendedY = originalY * (1.0 - blend) + optimizedY * blend;
                    pNewFoil->setBaseNode(i, x, blendedY);
                }
            }
        }

        // For symmetric mode, mirror upper surface to lower surface
        if(m_bSymmetric && nOptim > 2)
        {
            // Upper surface: indices 1 to leOptimIndex-1
            // Lower surface: indices leOptimIndex+1 to nOptim-2
            // Mirror: upper[k] <-> lower[nOptim-1-k]
            for(int i = 1; i < leOptimIndex; ++i)
            {
                const int mirrorIndex = nOptim - 1 - i;
                if(mirrorIndex > leOptimIndex && mirrorIndex < nOptim - 1)
                {
                    double y = pNewFoil->yb(i);
                    pNewFoil->setBaseNode(mirrorIndex, pNewFoil->xb(mirrorIndex), -y);
                }
            }
            // Set LE to y=0 for perfect symmetry
            pNewFoil->setBaseNode(leOptimIndex, pNewFoil->xb(leOptimIndex), 0.0);
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

void PSOTaskFoil::setPlane3D(PlaneXfl *pPlane, int wingIndex, int sectionIndex)
{
    m_pPlane3D = pPlane;
    m_WingIndex = wingIndex;
    m_SectionIndex = sectionIndex;
}

void PSOTaskFoil::setTargetCl(double cl)
{
    m_TargetMode = TargetMode::Cl;
    m_TargetValue = cl;
}


void PSOTaskFoil::getOptimMarkers(std::vector<std::pair<double, double>> &ctrlPts,
                                   std::vector<std::tuple<double, double, double>> &bounds) const
{
    ctrlPts.clear();
    bounds.clear();

    if(m_Preset == PresetType::V3_BSpline_Control)
    {
        // V3: Use B-spline control points
        const int nCtrl = m_BaseBSpline.nCtrlPoints();
        for(int i = 0; i < nCtrl; ++i)
        {
            Node2d pt = m_BaseBSpline.controlPoint(i);
            ctrlPts.push_back({pt.x, pt.y});
        }

        // Bounds for variable control points only
        for(int i = 0; i < int(m_VarToBase.size()); ++i)
        {
            const int ctrlIndex = m_VarToBase[i];
            if(ctrlIndex >= 0 && ctrlIndex < nCtrl)
            {
                Node2d pt = m_BaseBSpline.controlPoint(ctrlIndex);
                if(i < int(m_Variable.size()))
                {
                    double yMin = m_Variable[i].m_Min;
                    double yMax = m_Variable[i].m_Max;
                    bounds.push_back({pt.x, yMin, yMax});
                }
            }
        }
    }
    else if(m_Preset == PresetType::V1_Y_Only)
    {
        // V1: Use base foil nodes being optimized
        for(int i = 0; i < int(m_VarToBase.size()); ++i)
        {
            const int baseIndex = m_VarToBase[i];
            if(baseIndex >= 0 && baseIndex < int(m_OptimBaseNodes.size()))
            {
                double x = m_OptimBaseNodes[baseIndex].x;
                double y = m_OptimBaseNodes[baseIndex].y;
                ctrlPts.push_back({x, y});

                if(i < int(m_Variable.size()))
                {
                    double yMin = m_Variable[i].m_Min;
                    double yMax = m_Variable[i].m_Max;
                    bounds.push_back({x, yMin, yMax});
                }
            }
        }
    }
    // V2 (Camber/Thickness) doesn't have spatial control points
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
    m_VarIsX.clear();
    m_OptimBaseNodes.clear();
    m_OptimBaseIndex.clear();
    m_BaseLERadius = 0.0;
    m_BaseMaxLECurvature = 0.0;
    m_BaseMaxLETurnAngle = 0.0;
    m_BaseHasMonotonicLE = false;
    m_BaseHasPositiveThicknessLE = false;
    m_BaseHasSelfIntersection = false;

    if(!m_pFoil)
        return;

    auto applyMetrics = [this](const LeMetrics &metrics)
    {
        m_BaseLERadius = metrics.leRadius;
        m_BaseMaxLECurvature = metrics.maxCurvature;
        m_BaseMaxLETurnAngle = metrics.maxTurnAngle;
        m_BaseHasMonotonicLE = metrics.hasMonotonic;
        m_BaseHasPositiveThicknessLE = metrics.hasPositiveThickness;
        m_BaseHasSelfIntersection = metrics.hasSelfIntersection;
    };

    if(m_Preset == PresetType::V2_Camber_Thickness)
    {
        m_BaseMaxCamber = m_pFoil->maxCamber();
        m_BaseMaxThickness = m_pFoil->maxThickness();
        m_BaseXCamber = m_pFoil->xCamber();
        m_BaseXThickness = m_pFoil->xThickness();

        double delta;

        if(m_bSymmetric)
        {
            // Symmetric: Only optimize thickness (camber forced to zero)
            // 1. Max Thickness
            delta = std::max(0.005, std::fabs(m_BaseMaxThickness) * 0.5);
            m_Variable.emplace_back("MaxThickness", std::max(0.01, m_BaseMaxThickness - delta), m_BaseMaxThickness + delta);

            // 2. Max Thickness Position
            delta = 0.1;
            m_Variable.emplace_back("XThickness", std::max(0.1, m_BaseXThickness - delta), std::min(0.9, m_BaseXThickness + delta));
        }
        else
        {
            // 1. Max Camber
            delta = std::max(0.005, std::fabs(m_BaseMaxCamber) * 0.5);
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
        }

        LeMetrics baseMetrics;
        if(computeLeMetrics(*m_pFoil, baseMetrics))
            applyMetrics(baseMetrics);
        return;
    }

    if(m_Preset == PresetType::V3_BSpline_Control)
    {
        // V3: B-spline control point optimization
        // Control points act as "magnets" pulling the curve towards them (approximating, not interpolating)
        // This produces smoother results than directly moving foil base nodes

        const int nBase = m_pFoil->nBaseNodes();

        // B-spline approximation requires: nCtrlPts < nBaseNodes
        // Also need at least 8 control points for meaningful optimization
        if(nBase < 9)
            return; // Foil has too few base nodes for V3 optimization

        // Determine number of control points based on optimization points setting
        // Must be strictly less than nBaseNodes for approximation to work
        m_BSplineCtrlPts = (m_OptimizationPoints > 0) ? m_OptimizationPoints : std::min(20, nBase - 1);
        m_BSplineCtrlPts = std::max(8, std::min(m_BSplineCtrlPts, nBase - 1));
        m_BSplineDegree = 3; // Cubic B-spline
        m_BSplineOutputPts = std::max(100, nBase);

        // Create B-spline approximation of the original foil
        m_BaseBSpline.resetSpline();
        if(!m_pFoil->makeApproxBSpline(m_BaseBSpline, m_BSplineDegree, m_BSplineCtrlPts, m_BSplineOutputPts))
            return;

        const int nCtrl = m_BaseBSpline.nCtrlPoints();
        if(nCtrl < 4)
            return;

        // Find the LE control point (minimum x)
        m_BSplineLECtrlIndex = 0;
        double minX = m_BaseBSpline.controlPoint(0).x;
        for(int i = 1; i < nCtrl; ++i)
        {
            if(m_BaseBSpline.controlPoint(i).x < minX)
            {
                minX = m_BaseBSpline.controlPoint(i).x;
                m_BSplineLECtrlIndex = i;
            }
        }

        // Create optimization variables for each control point (except TE endpoints and LE)
        // The B-spline endpoints are clamped, so first and last control points coincide with TE
        // LE-adjacent points get both X and Y variables to allow proper LE shaping
        const double maxThickness = std::fabs(m_pFoil->maxThickness());
        double delta = (yDelta > 0.0) ? yDelta : std::max(0.005, 0.3 * maxThickness);
        delta *= m_BoundsScale;
        const double xDelta = delta * m_LEXBoundsScale;

        m_VarToBase.clear();
        m_VarIsX.clear();
        m_Variable.reserve(nCtrl * 2);  // Reserve for possible X+Y variables

        // Helper to check if control point is LE-adjacent
        auto isLEAdjacent = [this](int i) {
            const int distFromLE = std::abs(i - m_BSplineLECtrlIndex);
            return distFromLE > 0 && distFromLE <= m_LEXPoints;
        };

        if(m_bSymmetric)
        {
            // Symmetric: Only optimize upper surface (indices 1 to LEindex-1)
            // Lower surface will be mirrored in createOptimizedFoil
            for(int i = 1; i < m_BSplineLECtrlIndex; ++i)
            {
                const double x = m_BaseBSpline.controlPoint(i).x;
                const double y = m_BaseBSpline.controlPoint(i).y;

                // Add X variable for LE-adjacent points
                if(isLEAdjacent(i))
                {
                    m_Variable.emplace_back("cpx_" + std::to_string(i), x - xDelta, x + xDelta);
                    m_VarToBase.push_back(i);
                    m_VarIsX.push_back(true);
                }

                // Add Y variable for all points
                m_Variable.emplace_back("cpy_" + std::to_string(i), y - delta, y + delta);
                m_VarToBase.push_back(i);
                m_VarIsX.push_back(false);
            }
        }
        else
        {
            for(int i = 0; i < nCtrl; ++i)
            {
                // Skip TE endpoints (first and last) and LE control point
                if(i == 0 || i == nCtrl - 1 || i == m_BSplineLECtrlIndex)
                    continue;

                const double x = m_BaseBSpline.controlPoint(i).x;
                const double y = m_BaseBSpline.controlPoint(i).y;

                // Add X variable for LE-adjacent points
                if(isLEAdjacent(i))
                {
                    m_Variable.emplace_back("cpx_" + std::to_string(i), x - xDelta, x + xDelta);
                    m_VarToBase.push_back(i);
                    m_VarIsX.push_back(true);
                }

                // Add Y variable for all points
                m_Variable.emplace_back("cpy_" + std::to_string(i), y - delta, y + delta);
                m_VarToBase.push_back(i);
                m_VarIsX.push_back(false);
            }
        }

        // Compute LE metrics from the original foil for validation
        LeMetrics baseMetrics;
        if(computeLeMetrics(*m_pFoil, baseMetrics))
            applyMetrics(baseMetrics);
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
    double maxX = m_OptimBaseNodes.front().x;
    for(int i=1; i<int(m_OptimBaseNodes.size()); ++i)
    {
        if(m_OptimBaseNodes[i].x < minX)
        {
            minX = m_OptimBaseNodes[i].x;
            leOptimIndex = i;
        }
        if(m_OptimBaseNodes[i].x > maxX)
            maxX = m_OptimBaseNodes[i].x;
    }

    const double maxThickness = std::fabs(m_pFoil->maxThickness());
    double delta = (yDelta > 0.0) ? yDelta : std::max(0.002, 0.2 * maxThickness);
    delta *= m_BoundsScale;

    const int nOptim = int(m_OptimBaseNodes.size());
    m_Variable.reserve(nOptim);
    m_VarIsX.clear();

    // LE exclusion zone: skip variables for points too close to LE
    // High-leverage points near LE cause LE distortion when moved
    const double leX = minX;
    const double chord = maxX - minX;
    const double leExclusionX = leX + m_LEBlendChord * chord;

    if(m_bSymmetric)
    {
        // Symmetric: Only optimize upper surface (indices 1 to leOptimIndex-1)
        // Lower surface will be mirrored in createOptimizedFoil
        for(int i = 1; i < leOptimIndex; ++i)
        {
            const double x = m_OptimBaseNodes[i].x;
            const double y = m_OptimBaseNodes[i].y;

            // Skip points within LE exclusion zone
            if(x < leExclusionX)
                continue;

            // Add Y variable
            m_Variable.emplace_back("yb_" + std::to_string(m_OptimBaseIndex[i]), y - delta, y + delta);
            m_VarToBase.push_back(i);
            m_VarIsX.push_back(false);
        }
    }
    else
    {
        for(int i=0; i<nOptim; ++i)
        {
            if(i == 0 || i == nOptim-1 || i == leOptimIndex)
                continue;

            const double x = m_OptimBaseNodes[i].x;
            const double y = m_OptimBaseNodes[i].y;

            // Skip points within LE exclusion zone
            if(x < leExclusionX)
                continue;

            // Add Y variable
            m_Variable.emplace_back("yb_" + std::to_string(m_OptimBaseIndex[i]), y - delta, y + delta);
            m_VarToBase.push_back(i);
            m_VarIsX.push_back(false);
        }

        if(m_VarToBase.empty() && nOptim > 2)
        {
            int mid = nOptim/2;
            if(mid == 0 || mid == nOptim-1 || mid == leOptimIndex)
                mid = (mid+1 < nOptim-1) ? mid+1 : 1;

            const double y = m_OptimBaseNodes[mid].y;
            m_Variable.emplace_back("yb_" + std::to_string(m_OptimBaseIndex[mid]), y - delta, y + delta);
            m_VarToBase.push_back(mid);
            m_VarIsX.push_back(false);
        }
    }

    if(!m_OptimBaseNodes.empty())
    {
        Foil baseFoil;
        baseFoil.setBaseNodes(m_OptimBaseNodes);
        if(baseFoil.initGeometry())
        {
            LeMetrics baseMetrics;
            if(computeLeMetrics(baseFoil, baseMetrics))
                applyMetrics(baseMetrics);
        }
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
        double maxC, maxT, xC, xT;

        if(m_bSymmetric)
        {
            // Symmetric: only [MaxThickness, XThickness] variables, camber forced to 0
            if(pParticle->dimension() < 2) return;
            maxC = 0.0;
            maxT = pParticle->pos(0);
            xC = 0.3; // Fixed camber position (doesn't matter since camber = 0)
            xT = pParticle->pos(1);
        }
        else
        {
            if(pParticle->dimension() < 4) return;
            maxC = pParticle->pos(0);
            maxT = pParticle->pos(1);
            xC   = pParticle->pos(2);
            xT   = pParticle->pos(3);
        }

        workFoil.setCamber(xC, maxC);
        workFoil.setThickness(xT, maxT);
        workFoil.makeBaseFromCamberAndThickness();
        workFoil.rebuildPointSequenceFromBase();
    }
    else if(m_Preset == PresetType::V3_BSpline_Control)
    {
        // V3: Create work foil from modified B-spline control points
        const int nCtrl = m_BaseBSpline.nCtrlPoints();
        if(nCtrl < 4) return;

        // Copy the base B-spline and modify control point Y coordinates
        BSpline workBSpline;
        workBSpline.duplicate(m_BaseBSpline);

        // Apply particle position values to control points (X or Y depending on m_VarIsX)
        for(int i = 0; i < int(m_VarToBase.size()); ++i)
        {
            const int ctrlIndex = m_VarToBase[i];
            Node2d pt = workBSpline.controlPoint(ctrlIndex);
            if(i < int(m_VarIsX.size()) && m_VarIsX[i])
                pt.x = pParticle->pos(i);
            else
                pt.y = pParticle->pos(i);
            workBSpline.setCtrlPoint(ctrlIndex, pt);
        }

        // Generate the output curve from modified control points
        workBSpline.updateSpline();
        workBSpline.makeCurve();

        const std::vector<Node2d> &output = workBSpline.outputPts();
        if(output.size() < 10) return;

        // For symmetric mode, mirror at output curve level (not control points)
        if(m_bSymmetric)
        {
            // Find LE index in output curve (minimum X)
            int leIdx = 0;
            double minX = output[0].x;
            for(int i = 1; i < int(output.size()); ++i)
            {
                if(output[i].x < minX)
                {
                    minX = output[i].x;
                    leIdx = i;
                }
            }

            // Build symmetric foil: upper surface mirrored to lower
            // Output is ordered: TE(upper) -> LE -> TE(lower)
            // Upper: indices 0 to leIdx, Lower: indices leIdx to end
            std::vector<Node2d> symPts;
            symPts.reserve(output.size());

            // Copy upper surface (0 to leIdx)
            for(int i = 0; i <= leIdx; ++i)
                symPts.push_back(output[i]);

            // Set LE to y=0
            symPts[leIdx].y = 0.0;

            // Mirror upper surface to create lower (skip LE, go from leIdx-1 to 0)
            for(int i = leIdx - 1; i >= 0; --i)
            {
                Node2d pt = symPts[i];
                pt.y = -pt.y;
                symPts.push_back(pt);
            }

            workFoil.setBaseNodes(symPts);
        }
        else
        {
            workFoil.setBaseNodes(output);
        }
    }
    else // V1
    {
        if(!m_OptimBaseNodes.empty())
            workFoil.setBaseNodes(m_OptimBaseNodes);

        // Find LE index and chord in the optimized base nodes
        int leOptimIndex = 0;
        const int nOptim = int(m_OptimBaseNodes.size());
        double minX = m_OptimBaseNodes.empty() ? 0.0 : m_OptimBaseNodes[0].x;
        double maxX = minX;
        for(int i = 1; i < nOptim; ++i)
        {
            if(m_OptimBaseNodes[i].x < minX)
            {
                minX = m_OptimBaseNodes[i].x;
                leOptimIndex = i;
            }
            if(m_OptimBaseNodes[i].x > maxX)
                maxX = m_OptimBaseNodes[i].x;
        }
        const double chord = maxX - minX;

        // Apply particle position values (X or Y depending on m_VarIsX)
        for(int i=0; i<int(m_VarToBase.size()); ++i)
        {
            const int baseIndex = m_VarToBase[i];
            if(i < int(m_VarIsX.size()) && m_VarIsX[i])
                workFoil.setBaseNode(baseIndex, pParticle->pos(i), workFoil.yb(baseIndex));
            else
                workFoil.setBaseNode(baseIndex, workFoil.xb(baseIndex), pParticle->pos(i));
        }

        // LE blend: smoothly transition from original shape near LE to optimized shape
        if(m_LEBlendChord > 0.0 && chord > 0.0)
        {
            const double leBlendEnd = minX + m_LEBlendChord * chord;
            for(int i = 0; i < nOptim; ++i)
            {
                if(i == leOptimIndex)
                    continue;

                const double x = workFoil.xb(i);
                if(x < leBlendEnd)
                {
                    const double t = (x - minX) / (leBlendEnd - minX);
                    const double blend = t * t * (3.0 - 2.0 * t);  // smoothstep

                    const double originalY = m_OptimBaseNodes[i].y;
                    const double optimizedY = workFoil.yb(i);
                    const double blendedY = originalY * (1.0 - blend) + optimizedY * blend;
                    workFoil.setBaseNode(i, x, blendedY);
                }
            }
        }

        // For symmetric mode, mirror upper surface to lower surface
        if(m_bSymmetric && nOptim > 2)
        {
            for(int i = 1; i < leOptimIndex; ++i)
            {
                const int mirrorIndex = nOptim - 1 - i;
                if(mirrorIndex > leOptimIndex && mirrorIndex < nOptim - 1)
                {
                    double y = workFoil.yb(i);
                    workFoil.setBaseNode(mirrorIndex, workFoil.xb(mirrorIndex), -y);
                }
            }
            // Set LE to y=0 for perfect symmetry
            workFoil.setBaseNode(leOptimIndex, workFoil.xb(leOptimIndex), 0.0);
        }
    }

    if(!workFoil.initGeometry())
        return;

    if(!isFoilGeometryValid(workFoil,
                            m_BaseLERadius,
                            m_BaseMaxLECurvature,
                            m_BaseMaxLETurnAngle,
                            m_BaseHasMonotonicLE,
                            m_BaseHasPositiveThicknessLE,
                            m_BaseHasSelfIntersection))
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

        // Position-based thickness constraints (prevents flat trailing edges)
        if (m_Constraints.minThickAt80.enabled) {
            double t80 = workFoil.thickness(0.80);
            if (t80 < m_Constraints.minThickAt80.value)
                penalty += std::pow(m_Constraints.minThickAt80.value - t80, 2) * 5000.0;
        }

        if (m_Constraints.minThickAt90.enabled) {
            double t90 = workFoil.thickness(0.90);
            if (t90 < m_Constraints.minThickAt90.value)
                penalty += std::pow(m_Constraints.minThickAt90.value - t90, 2) * 5000.0;
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

    // Mode B: apply induced alpha correction for 3D effects
    // In 3D flow, the effective AoA at the section is alpha_geometric + alpha_induced
    // The induced alpha is typically negative (downwash reduces effective AoA)
    if (m_OptMode == OptimizationMode::ModeB && useAlpha && std::isfinite(m_InducedAlpha)) {
        target += m_InducedAlpha;  // alpha_effective = alpha_geometric + alpha_induced
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

            // Pressure gradient constraints (dCp/dx on upper surface)
            // Positive values indicate adverse pressure gradient (promotes separation)
            if (m_Constraints.maxDCpDxAt10.enabled) {
                double dCpDx = upperSurfaceDCpDx(workFoil, *pOpp, 0.10);
                if (std::isfinite(dCpDx) && dCpDx > m_Constraints.maxDCpDxAt10.value)
                    aeroPenalty += std::pow(dCpDx - m_Constraints.maxDCpDxAt10.value, 2) * 100.0;
            }
            if (m_Constraints.maxDCpDxAt25.enabled) {
                double dCpDx = upperSurfaceDCpDx(workFoil, *pOpp, 0.25);
                if (std::isfinite(dCpDx) && dCpDx > m_Constraints.maxDCpDxAt25.value)
                    aeroPenalty += std::pow(dCpDx - m_Constraints.maxDCpDxAt25.value, 2) * 100.0;
            }
            if (m_Constraints.maxDCpDxAt50.enabled) {
                double dCpDx = upperSurfaceDCpDx(workFoil, *pOpp, 0.50);
                if (std::isfinite(dCpDx) && dCpDx > m_Constraints.maxDCpDxAt50.value)
                    aeroPenalty += std::pow(dCpDx - m_Constraints.maxDCpDxAt50.value, 2) * 100.0;
            }
            if (m_Constraints.maxDCpDxAt75.enabled) {
                double dCpDx = upperSurfaceDCpDx(workFoil, *pOpp, 0.75);
                if (std::isfinite(dCpDx) && dCpDx > m_Constraints.maxDCpDxAt75.value)
                    aeroPenalty += std::pow(dCpDx - m_Constraints.maxDCpDxAt75.value, 2) * 100.0;
            }
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
