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

****************************************************************************/

/**
 * 3D solver smoke test.
 *
 * Creates a plane (following the proven planerun.cpp pattern),
 * runs analysis, and asserts that aerodynamic coefficients are
 * in physically reasonable ranges.
 */

#include <cmath>
#include <iostream>
#include <vector>
#include <cassert>

#include <api.h>
#include <constants.h>
#include <foil.h>
#include <objects2d.h>
#include <objects3d.h>
#include <planeopp.h>
#include <planepolar.h>
#include <planetask.h>
#include <planexfl.h>
#include <polar.h>
#include <oppoint.h>
#include <xfoiltask.h>

static bool isFinite(double v) { return std::isfinite(v); }

struct TestResult
{
    const char *name;
    bool passed;
};

int main()
{
    std::cout << "flow5 3D solver smoke test\n\n";

    std::vector<TestResult> results;

    // ---- Create foils (same pattern as planerun.cpp) ----
    Foil *pFoilN2413 = foil::makeNacaFoil(2413, "NACA 2413");
    Foil *pFoilN0009 = foil::makeNacaFoil(9,    "NACA 0009");
    if(!pFoilN0009 || !pFoilN2413)
    {
        std::cerr << "FAIL: could not create foils\n";
        globals::deleteObjects();
        return 1;
    }

    int npanels = 149;
    double amp = 0.7;
    pFoilN0009->rePanel(npanels, amp);
    pFoilN2413->rePanel(npanels, amp);
    pFoilN0009->setTEFlapData(true, 0.7, 0.5, 0.0);
    pFoilN2413->setTEFlapData(true, 0.7, 0.5, 0.0);

    // ---- Create plane (same as planerun.cpp) ----
    PlaneXfl *pPlane = new PlaneXfl;
    pPlane->setName("SmokeTestPlane");
    Objects3d::insertPlane(pPlane);
    pPlane->makeDefaultPlane();

    // Main wing
    WingXfl &mainwing = *pPlane->mainWing();
    for(int isec=0; isec<mainwing.nSections(); isec++)
    {
        WingSection &sec = mainwing.section(isec);
        sec.setLeftFoilName(pFoilN2413->name());
        sec.setRightFoilName(pFoilN2413->name());
        sec.setNX(13);
        sec.setXDistType(xfl::TANH);
    }
    mainwing.rootSection().setDihedral(3.5);
    mainwing.rootSection().setChord(0.27);
    mainwing.rootSection().setNY(13);
    mainwing.rootSection().setYDistType(xfl::UNIFORM);
    mainwing.tipSection().setYPosition(1.0);
    mainwing.tipSection().setChord(0.15);

    // Elevator
    WingXfl *pElev = pPlane->stab();
    pElev->setPosition(0.970, 0.0, 0.210);
    for(int isec=0; isec<pElev->nSections(); isec++)
    {
        WingSection &sec = pElev->section(isec);
        sec.setLeftFoilName(pFoilN0009->name());
        sec.setRightFoilName(pFoilN0009->name());
        sec.setNX(7);
        sec.setXDistType(xfl::TANH);
    }
    pElev->rootSection().setChord(0.13);
    pElev->tipSection().setYPosition(0.247);

    // Fin
    WingXfl &fin = *pPlane->fin();
    fin.setPosition(0.930, 0.0, 0.010);
    fin.setClosedInnerSide(true);
    for(int isec=0; isec<fin.nSections(); isec++)
    {
        WingSection &sec = fin.section(isec);
        sec.setLeftFoilName(pFoilN0009->name());
        sec.setRightFoilName(pFoilN0009->name());
        sec.setNX(7);
        sec.setXDistType(xfl::TANH);
    }
    fin.rootSection().setChord(0.19);
    fin.tipSection().setYPosition(0.17);
    fin.tipSection().setChord(0.09);

    // Assemble
    pPlane->makePlane(false, false, true);

    // =========================================================
    // Test 1: TRIUNIFORM analysis (proven method from planerun)
    // =========================================================
    std::cout << "Test 1: TRIUNIFORM analysis with assertions\n";
    {
        PlanePolar *pPolar = new PlanePolar;
        pPolar->setName("SmokeTest_TRI");
        Objects3d::insertPlPolar(pPolar);
        pPolar->setPlaneName(pPlane->name());
        pPolar->setType(xfl::T1POLAR);
        pPolar->setAnalysisMethod(xfl::TRIUNIFORM);
        pPolar->setReferenceDim(xfl::PROJECTED);
        pPolar->setReferenceArea(pPlane->projectedArea());
        pPolar->setReferenceSpanLength(pPlane->projectedSpan());
        pPolar->setReferenceChordLength(pPlane->mac());
        pPolar->setVelocity(20.0);  // 20 m/s free stream
        pPolar->setThinSurfaces(true);
        pPolar->setViscous(false);   // inviscid for speed

        // Resize flap controls to match wings (required when foils have flaps)
        pPolar->resizeFlapCtrls(pPlane);

        PlaneTask *pTask = new PlaneTask;
        pTask->outputToStdIO(false);
        pTask->setKeepOpps(true);
        pTask->setObjects(pPlane, pPolar);
        pTask->setComputeDerivatives(false);

        std::vector<double> alphas = {-2.0, 0.0, 2.0, 5.0, 8.0};
        pTask->setOppList(alphas);
        pTask->run();

        auto const &opps = pTask->planeOppList();
        bool ok = opps.size() >= 3;

        if(opps.empty())
        {
            std::cerr << "  FAIL: no operating points produced\n";
            ok = false;
        }

        double prevCL = -999.0;
        for(PlaneOpp const *pOpp : opps)
        {
            double alpha = pOpp->alpha();
            double CL = pOpp->aeroForces().CL();
            double CD = pOpp->aeroForces().CD();
            double Cm = pOpp->aeroForces().Cm();

            printf("  alpha=%5.1f  CL=%8.4f  CD=%8.5f  Cm=%8.4f\n", alpha, CL, CD, Cm);

            if(!isFinite(CL) || !isFinite(CD) || !isFinite(Cm))
            {
                std::cerr << "  FAIL: non-finite at alpha=" << alpha << "\n";
                ok = false;
                continue;
            }

            // CL should increase with alpha
            if(CL <= prevCL && prevCL != -999.0)
            {
                std::cerr << "  FAIL: CL not increasing\n";
                ok = false;
            }
            prevCL = CL;

            // At alpha=5, CL should be reasonable for this wing
            if(std::abs(alpha - 5.0) < 0.1)
            {
                if(CL < 0.2 || CL > 1.5)
                {
                    std::cerr << "  FAIL: CL=" << CL << " at alpha=5 out of range\n";
                    ok = false;
                }
            }

            // Induced drag must be non-negative
            double CDi = pOpp->aeroForces().CDi();
            if(CDi < -0.001)
            {
                std::cerr << "  FAIL: negative CDi=" << CDi << "\n";
                ok = false;
            }
        }

        std::cout << "  Points: " << opps.size() << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"TRIUNIFORM analysis", ok});
        delete pTask;
    }

    // =========================================================
    // Test 2: Plane geometry properties
    // =========================================================
    std::cout << "Test 2: Plane geometry properties\n";
    {
        bool ok = true;

        double area = pPlane->projectedArea();
        double span = pPlane->projectedSpan();
        double mac = pPlane->mac();

        printf("  Projected area: %8.4f m2\n", area);
        printf("  Projected span: %8.4f m\n", span);
        printf("  MAC:            %8.4f m\n", mac);

        if(area <= 0.0 || !isFinite(area))
        {
            std::cerr << "  FAIL: invalid area\n";
            ok = false;
        }
        if(span <= 0.0 || !isFinite(span))
        {
            std::cerr << "  FAIL: invalid span\n";
            ok = false;
        }
        if(mac <= 0.0 || !isFinite(mac))
        {
            std::cerr << "  FAIL: invalid MAC\n";
            ok = false;
        }

        // Span should be about 2.0m (1.0m semi-span * 2)
        if(span < 1.0 || span > 4.0)
        {
            std::cerr << "  FAIL: span out of expected range [1, 4]\n";
            ok = false;
        }

        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Plane geometry properties", ok});
    }

    // =========================================================
    // Summary
    // =========================================================
    std::cout << "=== SUMMARY ===\n";
    bool allPassed = true;
    for(auto const &r : results)
    {
        std::cout << "  " << r.name << ": " << (r.passed ? "PASS" : "FAIL") << "\n";
        if(!r.passed) allPassed = false;
    }
    std::cout << "===============\n";
    std::cout << "Overall: " << (allPassed ? "ALL PASS" : "SOME FAILED") << "\n";

    globals::deleteObjects();
    return allPassed ? 0 : 2;
}
