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

#include <cmath>
#include <iostream>
#include <vector>

#include <api.h>
#include <constants.h>
#include <objects2d.h>
#include <polar.h>
#include <foil.h>
#include <oppoint.h>
#include <xfoiltask.h>

static bool isFinite(double value)
{
    return std::isfinite(value);
}

struct TestResult
{
    const char *name;
    bool passed;
};

int main()
{
    printf("flow5 XFoil validation test\n\n");

    std::vector<TestResult> results;

    // =========================================================
    // Test 1: NACA 2410, Re=100000, validate Cl/Cd at key alphas
    // =========================================================
    std::cout << "Test 1: NACA 2410 Cl/Cd validation at Re=100k\n";
    {
        Foil *pFoil = foil::makeNacaFoil(2410, "NACA2410_test");
        if(!pFoil)
        {
            std::cerr << "  FAIL: could not create NACA 2410\n";
            globals::deleteObjects();
            return 1;
        }

        Polar *pPolar = Objects2d::createPolar(pFoil, xfl::T1POLAR, 100000.0, 0.0, 9.0, 1.0, 1.0);
        pPolar->setName("T1_validation");
        Objects2d::insertPolar(pPolar);

        bool bKeepOpps = true;
        XFoilTask task;
        task.initialize(*pFoil, pPolar, bKeepOpps);
        task.appendRange({true, 0.0, 10.0, 1.0});
        task.run();

        for(OpPoint *pOpp : task.operatingPoints())
            Objects2d::insertOpPoint(pOpp);

        // Collect results by alpha
        bool allOk = true;
        int nPoints = 0;
        bool foundAlpha0 = false;
        bool foundAlpha5 = false;

        for(OpPoint const *pOpp : Objects2d::operatingPoints())
        {
            if(pOpp->foilName() != pFoil->name()) continue;
            if(pOpp->polarName() != pPolar->name()) continue;
            nPoints++;

            double alpha = pOpp->m_Alpha;
            double cl = pOpp->m_Cl;
            double cd = pOpp->m_Cd;
            double cm = pOpp->m_Cm;

            printf("  alpha=%5.1f  Cl=%8.4f  Cd=%8.5f  Cm=%8.4f\n", alpha, cl, cd, cm);

            // All results must be finite
            if(!isFinite(cl) || !isFinite(cd) || !isFinite(cm))
            {
                std::cerr << "  FAIL: non-finite values at alpha=" << alpha << "\n";
                allOk = false;
                continue;
            }

            // Cd must be positive (drag is always positive)
            if(cd <= 0.0 || cd > 0.1)
            {
                std::cerr << "  FAIL: Cd=" << cd << " out of range (0, 0.1) at alpha=" << alpha << "\n";
                allOk = false;
            }

            // NACA 2410: 2% camber, so Cl at alpha=0 should be positive
            if(std::abs(alpha - 0.0) < 0.1)
            {
                foundAlpha0 = true;
                if(cl < 0.0 || cl > 0.6)
                {
                    std::cerr << "  FAIL: Cl=" << cl << " at alpha=0 (expected 0.0-0.6 for cambered foil)\n";
                    allOk = false;
                }
            }

            // At alpha=5 degrees, expect reasonable lift
            if(std::abs(alpha - 5.0) < 0.1)
            {
                foundAlpha5 = true;
                if(cl < 0.4 || cl > 1.4)
                {
                    std::cerr << "  FAIL: Cl=" << cl << " at alpha=5 (expected 0.4-1.4)\n";
                    allOk = false;
                }
            }

            // Cm should be negative for positive camber foils (nose-down moment)
            if(std::abs(alpha) < 0.1 && cm > 0.1)
            {
                std::cerr << "  FAIL: Cm=" << cm << " at alpha=0 (expected negative for cambered foil)\n";
                allOk = false;
            }
        }

        if(nPoints < 5)
        {
            std::cerr << "  FAIL: only " << nPoints << " converged points (expected >= 5 of 11)\n";
            allOk = false;
        }
        if(!foundAlpha0) { std::cerr << "  FAIL: alpha=0 point missing\n"; allOk = false; }
        if(!foundAlpha5) { std::cerr << "  FAIL: alpha=5 point missing\n"; allOk = false; }

        std::cout << "  Result: " << (allOk ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"NACA 2410 Cl/Cd validation", allOk});
    }

    // =========================================================
    // Test 2: Symmetric foil NACA 0012, Cl=0 at alpha=0
    // =========================================================
    std::cout << "Test 2: NACA 0012 symmetry check\n";
    {
        Foil *pFoil = foil::makeNacaFoil(12, "NACA0012_test");
        if(!pFoil)
        {
            std::cerr << "  FAIL: could not create NACA 0012\n";
            globals::deleteObjects();
            return 1;
        }

        Polar *pPolar = Objects2d::createPolar(pFoil, xfl::T1POLAR, 500000.0, 0.0, 9.0, 1.0, 1.0);
        pPolar->setName("T1_symm");
        Objects2d::insertPolar(pPolar);

        XFoilTask task;
        task.initialize(*pFoil, pPolar, true);
        task.appendRange({true, -2.0, 8.0, 1.0});
        task.run();

        for(OpPoint *pOpp : task.operatingPoints())
            Objects2d::insertOpPoint(pOpp);

        bool allOk = true;

        for(OpPoint const *pOpp : Objects2d::operatingPoints())
        {
            if(pOpp->foilName() != pFoil->name()) continue;
            if(pOpp->polarName() != pPolar->name()) continue;

            double alpha = pOpp->m_Alpha;
            double cl = pOpp->m_Cl;
            double cd = pOpp->m_Cd;

            printf("  alpha=%5.1f  Cl=%8.4f  Cd=%8.5f\n", alpha, cl, cd);

            if(!isFinite(cl) || !isFinite(cd))
            {
                std::cerr << "  FAIL: non-finite at alpha=" << alpha << "\n";
                allOk = false;
                continue;
            }

            // Symmetric foil at alpha=0 must have Cl near 0
            if(std::abs(alpha) < 0.1)
            {
                if(std::abs(cl) > 0.05)
                {
                    std::cerr << "  FAIL: Cl=" << cl << " at alpha=0 (expected ~0 for symmetric foil)\n";
                    allOk = false;
                }
            }

            // Cl should increase with alpha (lift slope ~0.1/deg)
            if(std::abs(alpha - 5.0) < 0.1)
            {
                if(cl < 0.3 || cl > 1.0)
                {
                    std::cerr << "  FAIL: Cl=" << cl << " at alpha=5 (expected 0.3-1.0)\n";
                    allOk = false;
                }
            }

            // Cd must be positive
            if(cd <= 0.0)
            {
                std::cerr << "  FAIL: negative Cd at alpha=" << alpha << "\n";
                allOk = false;
            }
        }

        std::cout << "  Result: " << (allOk ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"NACA 0012 symmetry check", allOk});
    }

    // =========================================================
    // Test 3: Negative alpha range
    // =========================================================
    std::cout << "Test 3: Negative alpha range\n";
    {
        Foil *pFoil = Objects2d::foil("NACA0012_test");
        Polar *pPolar = Objects2d::createPolar(pFoil, xfl::T1POLAR, 500000.0, 0.0, 9.0, 1.0, 1.0);
        pPolar->setName("T1_neg");
        Objects2d::insertPolar(pPolar);

        XFoilTask task;
        task.initialize(*pFoil, pPolar, true);
        task.appendRange({true, 0.0, -7.0, 1.0});
        task.run();

        for(OpPoint *pOpp : task.operatingPoints())
            Objects2d::insertOpPoint(pOpp);

        bool allOk = true;

        for(OpPoint const *pOpp : Objects2d::operatingPoints())
        {
            if(pOpp->polarName() != pPolar->name()) continue;

            double alpha = pOpp->m_Alpha;
            double cl = pOpp->m_Cl;

            printf("  alpha=%5.1f  Cl=%8.4f\n", alpha, cl);

            // At negative alpha, Cl should be negative for symmetric foil
            if(alpha < -1.0 && cl > 0.0)
            {
                std::cerr << "  FAIL: positive Cl at negative alpha=" << alpha << "\n";
                allOk = false;
            }
        }

        std::cout << "  Result: " << (allOk ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Negative alpha range", allOk});
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
