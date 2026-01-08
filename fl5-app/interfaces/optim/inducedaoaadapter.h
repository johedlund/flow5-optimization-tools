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

#pragma once

#include <string>

class PlaneXfl;
class WingXfl;
class PlanePolar;
class PlaneTask;

/**
 * @brief Adapter for extracting induced angle of attack from 3D panel analysis.
 *
 * Used by Mode B foil optimization to account for 3D wing effects when
 * optimizing a specific wing section.
 *
 * Usage:
 *   InducedAoAAdapter adapter;
 *   adapter.setPlane(plane, wingIndex, sectionIndex);
 *   adapter.setFlightConditions(alpha, velocity, density, viscosity);
 *   if (adapter.run()) {
 *       double alpha_induced = adapter.inducedAlpha();
 *       double alpha_effective = alpha + alpha_induced;
 *   }
 */
class InducedAoAAdapter
{
public:
    InducedAoAAdapter();
    ~InducedAoAAdapter();

    // Configuration
    void setPlane(PlaneXfl *pPlane, int wingIndex, int sectionIndex);
    void setFlightConditions(double alpha, double velocity, double density, double viscosity);
    void setNCrit(double ncrit) { m_NCrit = ncrit; }
    void setMach(double mach) { m_Mach = mach; }

    // Analysis
    bool run();

    // Results
    bool isValid() const { return m_bValid; }
    double inducedAlpha() const { return m_InducedAlpha; }
    double effectiveAlpha() const { return m_Alpha + m_InducedAlpha; }
    std::string const& lastError() const { return m_LastError; }
    std::string const& log() const { return m_Log; }

    // Accessors
    PlaneXfl* plane() const { return m_pPlane; }
    WingXfl* wing() const { return m_pWing; }
    int wingIndex() const { return m_WingIndex; }
    int sectionIndex() const { return m_SectionIndex; }
    double alpha() const { return m_Alpha; }

private:
    int findStationForSection() const;
    double interpolateInducedAlpha(int iStation, double ySection) const;

    PlaneXfl *m_pPlane = nullptr;
    WingXfl *m_pWing = nullptr;
    PlanePolar *m_pPolar = nullptr;
    PlaneTask *m_pTask = nullptr;

    int m_WingIndex = 0;
    int m_SectionIndex = 0;

    // Flight conditions
    double m_Alpha = 0.0;       // geometric AoA (degrees)
    double m_Velocity = 30.0;   // m/s
    double m_Density = 1.225;   // kg/m³
    double m_Viscosity = 1.5e-5; // m²/s (kinematic)
    double m_NCrit = 9.0;
    double m_Mach = 0.0;

    // Results
    bool m_bValid = false;
    double m_InducedAlpha = 0.0;
    std::string m_LastError;
    std::string m_Log;
};
