/****************************************************************************

    flow5 application
    Copyright (C) 2026 Johan Hedlund

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

#include "inducedaoaadapter.h"

#include <planexfl.h>
#include <wingxfl.h>
#include <planepolar.h>
#include <planetask.h>
#include <planeopp.h>
#include <wingopp.h>
#include <wingsection.h>
#include <panelanalysis.h>
#include <trimesh.h>

#include <cmath>
#include <sstream>

InducedAoAAdapter::InducedAoAAdapter()
{
}

InducedAoAAdapter::~InducedAoAAdapter()
{
    delete m_pPolar;
    delete m_pTask;
}

void InducedAoAAdapter::setPlane(PlaneXfl *pPlane, int wingIndex, int sectionIndex)
{
    m_pPlane = pPlane;
    m_WingIndex = wingIndex;
    m_SectionIndex = sectionIndex;
    m_bValid = false;

    if (m_pPlane)
    {
        // Get wing pointer based on index using generic accessor
        m_pWing = m_pPlane->wing(wingIndex);
    }
    else
    {
        m_pWing = nullptr;
    }
}

void InducedAoAAdapter::setFlightConditions(double alpha, double velocity, double density, double viscosity)
{
    m_Alpha = alpha;
    m_Velocity = velocity;
    m_Density = density;
    m_Viscosity = viscosity;
    m_bValid = false;
}

bool InducedAoAAdapter::run()
{
    m_bValid = false;
    m_InducedAlpha = 0.0;
    m_LastError.clear();
    m_Log.clear();

    std::ostringstream log;

    // Validate inputs
    if (!m_pPlane)
    {
        m_LastError = "No plane selected";
        return false;
    }

    if (!m_pWing)
    {
        m_LastError = "Invalid wing index: " + std::to_string(m_WingIndex);
        return false;
    }

    if (m_SectionIndex < 0 || m_SectionIndex >= m_pWing->nSections())
    {
        m_LastError = "Section index out of range: " + std::to_string(m_SectionIndex) +
                      " (wing has " + std::to_string(m_pWing->nSections()) + " sections)";
        return false;
    }

    log << "Running 3D analysis for induced AoA extraction\n";
    log << "  Wing: " << m_pWing->name() << "\n";
    log << "  Section: " << m_SectionIndex << " (y=" << m_pWing->section(m_SectionIndex).m_YPosition << "m)\n";
    log << "  Alpha: " << m_Alpha << " deg\n";
    log << "  Velocity: " << m_Velocity << " m/s\n";

    // Clean up previous analysis objects
    delete m_pPolar;
    delete m_pTask;

    // Create polar with flight conditions
    m_pPolar = new PlanePolar;
    m_pPolar->setDefaults();
    m_pPolar->setDensity(m_Density);
    m_pPolar->setViscosity(m_Viscosity);
    m_pPolar->setAnalysisMethod(xfl::VLM2);  // VLM2 is fast and gives induced angles
    m_pPolar->setViscous(false);              // Inviscid for speed - we only need induced AoA
    m_pPolar->setReferenceChordLength(m_pPlane->mac());
    m_pPolar->setReferenceArea(m_pPlane->projectedArea(false));
    m_pPolar->setReferenceSpanLength(m_pPlane->projectedSpan());

    // Build plane mesh
    m_pPlane->makePlane(m_pPolar->bThickSurfaces(), true, m_pPolar->isTriangleMethod());

    // Create and configure task
    m_pTask = new PlaneTask;
    PanelAnalysis::setMultiThread(false);  // Single-threaded for safety in optimization loop
    Task3d::setCancelled(false);
    TriMesh::setCancelled(false);

    m_pTask->setAnalysisStatus(xfl::RUNNING);
    m_pTask->setObjects(m_pPlane, m_pPolar);
    m_pTask->setComputeDerivatives(false);
    m_pTask->setOppList({m_Alpha});

    // Run analysis
    m_pTask->run();

    // Check results
    if (m_pTask->planeOppList().empty())
    {
        m_LastError = "3D analysis produced no results - may have diverged";
        m_Log = log.str();
        return false;
    }

    PlaneOpp *pPOpp = m_pTask->planeOppList().front();
    if (!pPOpp || !pPOpp->hasWOpp() || m_WingIndex >= pPOpp->nWOpps())
    {
        m_LastError = "3D analysis results incomplete - no wing operating point";
        m_Log = log.str();
        return false;
    }

    // Extract induced AoA at the target section
    WingOpp const &wopp = pPOpp->WOpp(m_WingIndex);
    SpanDistribs const &sd = wopp.spanResults();

    if (sd.nStations() == 0)
    {
        m_LastError = "No span stations in results";
        m_Log = log.str();
        return false;
    }

    // Find the station closest to our target section
    int iStation = findStationForSection();
    if (iStation < 0 || iStation >= sd.nStations())
    {
        m_LastError = "Could not find station for section " + std::to_string(m_SectionIndex);
        m_Log = log.str();
        return false;
    }

    // Get induced angle (interpolate if between stations)
    double ySection = m_pWing->section(m_SectionIndex).m_YPosition;
    m_InducedAlpha = interpolateInducedAlpha(iStation, ySection);

    // Validate result
    if (!std::isfinite(m_InducedAlpha))
    {
        m_LastError = "Invalid induced AoA value (NaN or Inf)";
        m_InducedAlpha = 0.0;
        m_Log = log.str();
        return false;
    }

    log << "  Induced AoA: " << m_InducedAlpha << " deg\n";
    log << "  Effective AoA: " << effectiveAlpha() << " deg\n";

    m_bValid = true;
    m_Log = log.str();

    // Clean up PlaneOpp
    delete pPOpp;

    return true;
}

int InducedAoAAdapter::findStationForSection() const
{
    if (!m_pWing || !m_pTask || m_pTask->planeOppList().empty())
        return -1;

    PlaneOpp *pPOpp = m_pTask->planeOppList().front();
    if (!pPOpp || m_WingIndex >= pPOpp->nWOpps())
        return -1;

    SpanDistribs const &sd = pPOpp->WOpp(m_WingIndex).spanResults();
    if (sd.nStations() == 0)
        return -1;

    double ySection = m_pWing->section(m_SectionIndex).m_YPosition;

    // Find closest station by y-position
    int bestStation = 0;
    double bestDist = std::abs(sd.m_StripPos[0] - ySection);

    for (int i = 1; i < sd.nStations(); ++i)
    {
        double dist = std::abs(sd.m_StripPos[i] - ySection);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestStation = i;
        }
    }

    return bestStation;
}

double InducedAoAAdapter::interpolateInducedAlpha(int iStation, double ySection) const
{
    if (!m_pTask || m_pTask->planeOppList().empty())
        return 0.0;

    PlaneOpp *pPOpp = m_pTask->planeOppList().front();
    if (!pPOpp || m_WingIndex >= pPOpp->nWOpps())
        return 0.0;

    SpanDistribs const &sd = pPOpp->WOpp(m_WingIndex).spanResults();
    int n = sd.nStations();

    if (n == 0)
        return 0.0;

    if (n == 1 || iStation <= 0)
        return sd.m_Ai[iStation];

    if (iStation >= n - 1)
        return sd.m_Ai[n - 1];

    // Linear interpolation between adjacent stations
    double y0 = sd.m_StripPos[iStation];
    double y1 = sd.m_StripPos[iStation + 1];

    // Check if we should interpolate with previous station instead
    if (ySection < y0 && iStation > 0)
    {
        y1 = y0;
        y0 = sd.m_StripPos[iStation - 1];
        iStation--;
    }

    double dy = y1 - y0;
    if (std::abs(dy) < 1e-9)
        return sd.m_Ai[iStation];

    double t = (ySection - y0) / dy;
    t = std::max(0.0, std::min(1.0, t));  // Clamp to [0,1]

    return sd.m_Ai[iStation] * (1.0 - t) + sd.m_Ai[iStation + 1] * t;
}
