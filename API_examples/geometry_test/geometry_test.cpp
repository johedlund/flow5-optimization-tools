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
 * Regression tests for foil geometry validation.
 *
 * Tests documented failure modes from docs/lessons-learned.md:
 * - Self-intersecting foils must be detected
 * - Extreme Y-offsets near LE must cause rejection in optimization
 * - Normal NACA foils must pass all geometry checks
 */

#include <cmath>
#include <iostream>
#include <vector>

#include <QCoreApplication>

#include <api.h>
#include <foil.h>
#include <objects2d.h>
#include <polar.h>

#include <interfaces/optim/particle.h>
#include <interfaces/optim/psotaskfoil.h>

struct TestResult
{
    const char *name;
    bool passed;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    std::cout << "flow5 foil geometry regression test\n\n";

    std::vector<TestResult> results;

    // =========================================================
    // Test 1: Normal NACA 2412 passes geometry checks
    // =========================================================
    std::cout << "Test 1: Normal NACA 2412 geometry is valid\n";
    {
        Foil *pFoil = foil::makeNacaFoil(2412, "geom_2412");
        if(!pFoil)
        {
            std::cerr << "  FAIL: could not create foil\n";
            globals::deleteObjects();
            return 1;
        }

        Polar *pPolar = Objects2d::createPolar(pFoil, xfl::T1POLAR, 1000000.0, 0.0, 9.0, 1.0, 1.0);
        pPolar->setName("geom_polar");
        pPolar->setAoaSpec(2.0);
        Objects2d::insertPolar(pPolar);

        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        // Create particle at baseline (unmodified foil)
        Particle particle;
        particle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
            particle.setPos(i, task.variableBaseValue(i));

        PSOTask *base = &task;
        base->calcFitness(&particle, false, false);

        const double fitness = particle.fitness(0);
        const bool ok = particle.isConverged()
                     && std::isfinite(fitness)
                     && fitness > 0.0
                     && fitness != OPTIM_PENALTY;

        std::cout << "  Converged: " << (particle.isConverged() ? "true" : "false") << "\n";
        std::cout << "  Fitness: " << fitness << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Normal NACA 2412 geometry", ok});
    }

    // =========================================================
    // Test 2: Extreme LE offsets cause rejection (lessons-learned.md)
    // =========================================================
    std::cout << "Test 2: Extreme LE offsets cause penalty\n";
    {
        Foil *pFoil = Objects2d::foil("geom_2412");
        Polar *pPolar = Objects2d::polar(pFoil->name(), "geom_polar");

        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        // Apply extreme alternating offsets to create self-intersection
        Particle badParticle;
        badParticle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
        {
            double y = task.variableBaseValue(i);
            badParticle.setPos(i, (i % 2 == 0) ? y + 0.5 : y - 0.5);
        }

        PSOTask *base = &task;
        base->calcFitness(&badParticle, false, false);

        const double fitness = badParticle.fitness(0);
        const bool ok = (!badParticle.isConverged() && fitness == OPTIM_PENALTY);

        std::cout << "  Converged: " << (badParticle.isConverged() ? "true" : "false") << "\n";
        std::cout << "  Fitness: " << fitness << " (penalty=" << OPTIM_PENALTY << ")\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Extreme LE offsets rejected", ok});
    }

    // =========================================================
    // Test 3: Moderate offsets still produce valid foil
    // =========================================================
    std::cout << "Test 3: Moderate offsets produce valid foil\n";
    {
        Foil *pFoil = Objects2d::foil("geom_2412");
        Polar *pPolar = Objects2d::polar(pFoil->name(), "geom_polar");

        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        // Small uniform offsets should produce a slightly thicker but still valid foil
        Particle modParticle;
        modParticle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
        {
            double y = task.variableBaseValue(i);
            // Shift all points outward by a tiny amount (thickening)
            double sign = (y >= 0.0) ? 1.0 : -1.0;
            modParticle.setPos(i, y + sign * 0.002);
        }

        PSOTask *base = &task;
        base->calcFitness(&modParticle, false, false);

        const double fitness = modParticle.fitness(0);
        const bool ok = modParticle.isConverged()
                     && std::isfinite(fitness)
                     && fitness > 0.0
                     && fitness != OPTIM_PENALTY;

        std::cout << "  Converged: " << (modParticle.isConverged() ? "true" : "false") << "\n";
        std::cout << "  Fitness: " << fitness << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Moderate offsets valid", ok});
    }

    // =========================================================
    // Test 4: Foil thickness/camber properties are reasonable
    // =========================================================
    std::cout << "Test 4: NACA 2412 geometry properties\n";
    {
        Foil *pFoil = Objects2d::foil("geom_2412");
        bool ok = true;

        double thickness = pFoil->maxThickness();
        double camber = pFoil->maxCamber();

        std::cout << "  Max thickness: " << thickness << " (expected ~0.12)\n";
        std::cout << "  Max camber: " << camber << " (expected ~0.02)\n";

        // NACA 2412: 12% thickness, 2% camber
        if(thickness < 0.10 || thickness > 0.14)
        {
            std::cerr << "  FAIL: thickness=" << thickness << " out of range [0.10, 0.14]\n";
            ok = false;
        }
        if(camber < 0.01 || camber > 0.03)
        {
            std::cerr << "  FAIL: camber=" << camber << " out of range [0.01, 0.03]\n";
            ok = false;
        }

        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"NACA 2412 geometry properties", ok});
    }

    // =========================================================
    // Test 5: NACA 0012 symmetric foil has zero camber
    // =========================================================
    std::cout << "Test 5: NACA 0012 symmetric properties\n";
    {
        Foil *pFoil = foil::makeNacaFoil(12, "geom_0012");
        bool ok = true;

        double thickness = pFoil->maxThickness();
        double camber = pFoil->maxCamber();

        std::cout << "  Max thickness: " << thickness << " (expected ~0.12)\n";
        std::cout << "  Max camber: " << camber << " (expected ~0.00)\n";

        if(thickness < 0.10 || thickness > 0.14)
        {
            std::cerr << "  FAIL: thickness out of range\n";
            ok = false;
        }
        // Symmetric foil must have near-zero camber
        if(std::abs(camber) > 0.005)
        {
            std::cerr << "  FAIL: camber=" << camber << " should be ~0 for symmetric foil\n";
            ok = false;
        }

        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"NACA 0012 symmetric properties", ok});
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
