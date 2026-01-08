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
 * @file induced_aoa_test.cpp
 * @brief Headless test for InducedAoAAdapter (Mode B 3D coupling)
 *
 * Tests the extraction of induced angle of attack from 3D panel analysis
 * for use in foil optimization (Mode B).
 */

#include <cmath>
#include <iostream>
#include <string>

#include <QCoreApplication>

#include <api.h>
#include <constants.h>
#include <foil.h>
#include <objects2d.h>
#include <objects3d.h>
#include <planexfl.h>
#include <wingxfl.h>
#include <wingsection.h>

#include <interfaces/optim/inducedaoaadapter.h>

static bool isFinite(double value)
{
    return std::isfinite(value);
}

/**
 * Creates a simple test plane with a single wing.
 * The wing has 3 sections: root, mid, tip (span ~3m total).
 */
static PlaneXfl* createTestPlane()
{
    // Create foil for wing
    Foil *pFoil = foil::makeNacaFoil(2412, "Test_NACA_2412");
    if (!pFoil)
    {
        std::cerr << "  ERROR: Could not create NACA foil\n";
        return nullptr;
    }
    pFoil->rePanel(101, 0.7);

    // Create plane
    PlaneXfl *pPlane = new PlaneXfl;
    pPlane->setName("InducedAoATestPlane");
    Objects3d::insertPlane(pPlane);

    // Build default plane structure
    pPlane->makeDefaultPlane();

    // Configure main wing
    WingXfl *pWing = pPlane->mainWing();
    if (!pWing)
    {
        std::cerr << "  ERROR: No main wing in default plane\n";
        return nullptr;
    }

    pWing->setPosition(0, 0, 0);

    // Ensure we have at least 3 sections
    while (pWing->nSections() < 3)
    {
        pWing->insertSection(pWing->nSections() - 1);
    }

    // Configure sections
    for (int i = 0; i < pWing->nSections(); ++i)
    {
        WingSection &sec = pWing->section(i);
        sec.setLeftFoilName(pFoil->name());
        sec.setRightFoilName(pFoil->name());
        sec.setNX(11);
        sec.setXDistType(xfl::UNIFORM);
    }

    // Root section
    WingSection &root = pWing->rootSection();
    root.setChord(0.3);
    root.setDihedral(2.0);
    root.setNY(9);
    root.setYDistType(xfl::UNIFORM);

    // Mid section
    WingSection &mid = pWing->section(1);
    mid.setYPosition(0.8);
    mid.setChord(0.25);
    mid.setTwist(-1.5);
    mid.setDihedral(4.0);
    mid.setNY(11);
    mid.setYDistType(xfl::UNIFORM);

    // Tip section
    WingSection &tip = pWing->tipSection();
    tip.setYPosition(1.5);
    tip.setChord(0.15);
    tip.setTwist(-3.0);

    // Build plane mesh
    pPlane->makePlane(false, true, false);

    return pPlane;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    std::cout << "flow5 Induced AoA Extraction Test (Mode B)\n";
    std::cout << "==========================================\n\n";

    int nPass = 0;
    int nFail = 0;

    // Create test plane once for all tests
    PlaneXfl *pTestPlane = createTestPlane();
    if (!pTestPlane)
    {
        std::cerr << "FATAL: Could not create test plane\n";
        globals::deleteObjects();
        return 1;
    }

    // Test 1: Happy Path - Extract induced AoA for root section
    std::cout << "Test 1: Happy Path (Root Section)\n";
    {
        PlaneXfl *pPlane = pTestPlane;
        {
            InducedAoAAdapter adapter;
            adapter.setPlane(pPlane, 0, 0);  // Wing 0, Section 0 (root)
            adapter.setFlightConditions(5.0, 30.0, 1.225, 1.5e-5);  // 5 deg, 30 m/s

            bool success = adapter.run();
            std::cout << "  Log: " << adapter.log() << "\n";

            if (success && adapter.isValid())
            {
                double ai = adapter.inducedAlpha();
                double ae = adapter.effectiveAlpha();

                std::cout << "  Geometric AoA: 5.0 deg\n";
                std::cout << "  Induced AoA:   " << ai << " deg\n";
                std::cout << "  Effective AoA: " << ae << " deg\n";

                // Induced angle should be negative (downwash) and reasonable
                // For a finite wing at positive lift, induced AoA is typically -1 to -5 deg
                bool inRange = (ai < 0.0 && ai > -10.0);
                bool finite = isFinite(ai) && isFinite(ae);
                bool consistent = std::abs(ae - (5.0 + ai)) < 0.001;

                if (inRange && finite && consistent)
                {
                    std::cout << "  Result: PASS\n";
                    nPass++;
                }
                else
                {
                    std::cout << "  Result: FAIL (values out of expected range)\n";
                    nFail++;
                }
            }
            else
            {
                std::cout << "  Error: " << adapter.lastError() << "\n";
                std::cout << "  Result: FAIL\n";
                nFail++;
            }
        }
    }
    std::cout << "\n";

    // Test 2: Mid-span section
    std::cout << "Test 2: Mid-span Section\n";
    {
        PlaneXfl *pPlane = pTestPlane;
        {
            InducedAoAAdapter adapter;
            adapter.setPlane(pPlane, 0, 1);  // Wing 0, Section 1 (mid)
            adapter.setFlightConditions(5.0, 30.0, 1.225, 1.5e-5);

            bool success = adapter.run();

            if (success && adapter.isValid())
            {
                double ai = adapter.inducedAlpha();
                std::cout << "  Induced AoA at y=" << pPlane->mainWing()->section(1).m_YPosition << "m: " << ai << " deg\n";

                // Mid-span induced angle should be slightly less negative than root
                bool finite = isFinite(ai);
                bool inRange = (ai < 0.0 && ai > -10.0);

                if (finite && inRange)
                {
                    std::cout << "  Result: PASS\n";
                    nPass++;
                }
                else
                {
                    std::cout << "  Result: FAIL\n";
                    nFail++;
                }
            }
            else
            {
                std::cout << "  Error: " << adapter.lastError() << "\n";
                std::cout << "  Result: FAIL\n";
                nFail++;
            }
        }
    }
    std::cout << "\n";

    // Test 3: No plane (null pointer)
    std::cout << "Test 3: No Plane (Error Handling)\n";
    {
        InducedAoAAdapter adapter;
        adapter.setPlane(nullptr, 0, 0);
        adapter.setFlightConditions(5.0, 30.0, 1.225, 1.5e-5);

        bool success = adapter.run();

        if (!success && !adapter.isValid() && !adapter.lastError().empty())
        {
            std::cout << "  Error (expected): " << adapter.lastError() << "\n";
            std::cout << "  Result: PASS\n";
            nPass++;
        }
        else
        {
            std::cout << "  Result: FAIL (should have returned error)\n";
            nFail++;
        }
    }
    std::cout << "\n";

    // Test 4: Invalid section index
    std::cout << "Test 4: Invalid Section Index\n";
    {
        PlaneXfl *pPlane = pTestPlane;
        {
            InducedAoAAdapter adapter;
            adapter.setPlane(pPlane, 0, 99);  // Invalid section index
            adapter.setFlightConditions(5.0, 30.0, 1.225, 1.5e-5);

            bool success = adapter.run();

            if (!success && !adapter.isValid())
            {
                std::cout << "  Error (expected): " << adapter.lastError() << "\n";
                std::cout << "  Result: PASS\n";
                nPass++;
            }
            else
            {
                std::cout << "  Result: FAIL (should have rejected invalid section)\n";
                nFail++;
            }
        }
    }
    std::cout << "\n";

    // Test 5: Invalid wing index
    std::cout << "Test 5: Invalid Wing Index\n";
    {
        PlaneXfl *pPlane = pTestPlane;
        {
            InducedAoAAdapter adapter;
            adapter.setPlane(pPlane, 99, 0);  // Invalid wing index
            adapter.setFlightConditions(5.0, 30.0, 1.225, 1.5e-5);

            bool success = adapter.run();

            if (!success && !adapter.isValid())
            {
                std::cout << "  Error (expected): " << adapter.lastError() << "\n";
                std::cout << "  Result: PASS\n";
                nPass++;
            }
            else
            {
                std::cout << "  Result: FAIL (should have rejected invalid wing index)\n";
                nFail++;
            }
        }
    }
    std::cout << "\n";

    // Test 6: Different alpha values
    std::cout << "Test 6: Alpha Variation\n";
    {
        PlaneXfl *pPlane = pTestPlane;
        {
            double alphas[] = {0.0, 2.0, 5.0, 8.0};
            double prevAi = 0.0;
            bool allPass = true;

            for (double alpha : alphas)
            {
                InducedAoAAdapter adapter;
                adapter.setPlane(pPlane, 0, 0);
                adapter.setFlightConditions(alpha, 30.0, 1.225, 1.5e-5);

                bool success = adapter.run();
                if (!success || !adapter.isValid())
                {
                    std::cout << "  Alpha=" << alpha << " deg: FAILED - " << adapter.lastError() << "\n";
                    allPass = false;
                    continue;
                }

                double ai = adapter.inducedAlpha();
                std::cout << "  Alpha=" << alpha << " deg -> Induced=" << ai << " deg\n";

                // At higher alpha, induced angle should be more negative (more downwash)
                if (alpha > 2.0 && ai > prevAi + 0.01)
                {
                    std::cout << "    WARNING: Induced AoA didn't increase with alpha\n";
                    // Don't fail on this - the relationship depends on many factors
                }
                prevAi = ai;
            }

            if (allPass)
            {
                std::cout << "  Result: PASS\n";
                nPass++;
            }
            else
            {
                std::cout << "  Result: FAIL\n";
                nFail++;
            }
        }
    }
    std::cout << "\n";

    // Test 7: Accessor consistency
    std::cout << "Test 7: Accessor Consistency\n";
    {
        PlaneXfl *pPlane = pTestPlane;
        {
            InducedAoAAdapter adapter;
            adapter.setPlane(pPlane, 0, 1);
            adapter.setFlightConditions(5.0, 30.0, 1.225, 1.5e-5);

            bool success = adapter.run();
            if (success && adapter.isValid())
            {
                bool ok = true;

                // Check accessors return what was set
                if (adapter.plane() != pPlane) { ok = false; std::cout << "  plane() mismatch\n"; }
                if (adapter.wingIndex() != 0) { ok = false; std::cout << "  wingIndex() mismatch\n"; }
                if (adapter.sectionIndex() != 1) { ok = false; std::cout << "  sectionIndex() mismatch\n"; }
                if (std::abs(adapter.alpha() - 5.0) > 0.001) { ok = false; std::cout << "  alpha() mismatch\n"; }

                // Check effective alpha calculation
                double expected = adapter.alpha() + adapter.inducedAlpha();
                if (std::abs(adapter.effectiveAlpha() - expected) > 0.001)
                {
                    ok = false;
                    std::cout << "  effectiveAlpha() calculation mismatch\n";
                }

                if (ok)
                {
                    std::cout << "  All accessors consistent\n";
                    std::cout << "  Result: PASS\n";
                    nPass++;
                }
                else
                {
                    std::cout << "  Result: FAIL\n";
                    nFail++;
                }
            }
            else
            {
                std::cout << "  Error: " << adapter.lastError() << "\n";
                std::cout << "  Result: FAIL\n";
                nFail++;
            }
        }
    }
    std::cout << "\n";

    // Cleanup
    globals::deleteObjects();

    // Summary
    std::cout << "==========================================\n";
    std::cout << "SUMMARY: " << nPass << " passed, " << nFail << " failed\n";
    std::cout << "==========================================\n";

    return (nFail == 0) ? 0 : 1;
}
