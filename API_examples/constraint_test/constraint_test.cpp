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
 * Constraint enforcement test for the PSO optimization system.
 *
 * Validates that:
 * - Thickness constraints reject foils that are too thin/thick
 * - Camber constraints reject foils with excessive camber
 * - Constraints don't reject valid foils
 * - Rejection counts are tracked correctly
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

static bool isPenalty(double fitness)
{
    // OPTIM_PENALTY is 1e12; fitness may be slightly above due to accumulated terms
    return fitness >= OPTIM_PENALTY * 0.99;
}

struct TestResult
{
    const char *name;
    bool passed;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    std::cout << "flow5 constraint enforcement test\n\n";

    std::vector<TestResult> results;

    // Setup: NACA 2412 at Re=1e6
    Foil *pFoil = foil::makeNacaFoil(2412, "constr_2412");
    if(!pFoil)
    {
        std::cerr << "FAIL: could not create NACA foil\n";
        return 1;
    }

    Polar *pPolar = Objects2d::createPolar(pFoil, xfl::T1POLAR, 1000000.0, 0.0, 9.0, 1.0, 1.0);
    pPolar->setName("constr_polar");
    pPolar->setAoaSpec(2.0);
    Objects2d::insertPolar(pPolar);

    // =========================================================
    // Test 1: No constraints - baseline should pass
    // =========================================================
    std::cout << "Test 1: Baseline without constraints\n";
    {
        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        Particle particle;
        particle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
            particle.setPos(i, task.variableBaseValue(i));

        PSOTask *base = &task;
        base->calcFitness(&particle, false, false);

        const bool ok = particle.isConverged() && !isPenalty(particle.fitness(0));
        std::cout << "  Converged: " << (particle.isConverged() ? "true" : "false") << "\n";
        std::cout << "  Fitness: " << particle.fitness(0) << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Baseline without constraints", ok});
    }

    // =========================================================
    // Test 2: Min thickness constraint rejects thin foil target
    // NACA 2412 has ~12% thickness, so setting min=0.15 should reject
    // =========================================================
    std::cout << "Test 2: Min thickness constraint (0.15 > actual 0.12)\n";
    {
        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        // Set constraint: min thickness > actual thickness
        PSOTaskFoil::Constraints constr;
        constr.enabled = true;
        constr.minThickness = {0.15, true};  // 15% > NACA 2412's 12%
        task.setConstraints(constr);
        task.resetRejectionCounts();

        Particle particle;
        particle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
            particle.setPos(i, task.variableBaseValue(i));

        PSOTask *base = &task;
        base->calcFitness(&particle, false, false);

        const double fitness = particle.fitness(0);
        int minThickCount = task.rejectionCount(PSOTaskFoil::ConstraintType::MinThickness);

        const bool ok = isPenalty(fitness) && (minThickCount > 0);
        std::cout << "  Fitness: " << fitness << " (expected " << OPTIM_PENALTY << ")\n";
        std::cout << "  MinThickness rejections: " << minThickCount << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Min thickness rejects thin foil", ok});
    }

    // =========================================================
    // Test 3: Max thickness constraint rejects thick foil target
    // Setting max=0.08 should reject NACA 2412 (12% thick)
    // =========================================================
    std::cout << "Test 3: Max thickness constraint (0.08 < actual 0.12)\n";
    {
        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        PSOTaskFoil::Constraints constr;
        constr.enabled = true;
        constr.maxThickness = {0.08, true};  // 8% < NACA 2412's 12%
        task.setConstraints(constr);
        task.resetRejectionCounts();

        Particle particle;
        particle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
            particle.setPos(i, task.variableBaseValue(i));

        PSOTask *base = &task;
        base->calcFitness(&particle, false, false);

        const double fitness = particle.fitness(0);
        int maxThickCount = task.rejectionCount(PSOTaskFoil::ConstraintType::MaxThickness);

        const bool penaltyOk = isPenalty(fitness);
        const bool countOk = (maxThickCount > 0);
        const bool ok = penaltyOk && countOk;
        std::cout << "  Fitness: " << fitness << " (expected >= " << OPTIM_PENALTY << ")\n";
        std::cout << "  MaxThickness rejections: " << maxThickCount << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Max thickness rejects thick foil", ok});
    }

    // =========================================================
    // Test 4: Max camber constraint rejects high-camber foil
    // NACA 2412 has 2% camber, setting max=0.01 should reject
    // =========================================================
    std::cout << "Test 4: Max camber constraint (0.01 < actual 0.02)\n";
    {
        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        PSOTaskFoil::Constraints constr;
        constr.enabled = true;
        constr.maxCamber = {0.01, true};  // 1% < NACA 2412's 2%
        task.setConstraints(constr);
        task.resetRejectionCounts();

        Particle particle;
        particle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
            particle.setPos(i, task.variableBaseValue(i));

        PSOTask *base = &task;
        base->calcFitness(&particle, false, false);

        const double fitness = particle.fitness(0);
        int maxCamberCount = task.rejectionCount(PSOTaskFoil::ConstraintType::MaxCamber);

        const bool ok = isPenalty(fitness) && (maxCamberCount > 0);
        std::cout << "  Fitness: " << fitness << " (expected " << OPTIM_PENALTY << ")\n";
        std::cout << "  MaxCamber rejections: " << maxCamberCount << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Max camber rejects high-camber foil", ok});
    }

    // =========================================================
    // Test 5: Satisfied constraints don't reject valid foil
    // Set generous constraints that NACA 2412 easily satisfies
    // =========================================================
    std::cout << "Test 5: Satisfied constraints pass valid foil\n";
    {
        PSOTaskFoil task;
        task.setFoil(pFoil);
        task.setPolar(pPolar);
        task.initVariablesFromFoil();
        task.setTargetAlpha(2.0);
        task.setNObjectives(1);
        task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
        task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
        task.setTargetCl(0.73);

        PSOTaskFoil::Constraints constr;
        constr.enabled = true;
        constr.minThickness = {0.05, true};   // 5% < 12% - easily satisfied
        constr.maxThickness = {0.20, true};   // 20% > 12% - easily satisfied
        constr.maxCamber = {0.05, true};      // 5% > 2% - easily satisfied
        task.setConstraints(constr);
        task.resetRejectionCounts();

        Particle particle;
        particle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
        for(int i=0; i<task.nVariables(); ++i)
            particle.setPos(i, task.variableBaseValue(i));

        PSOTask *base = &task;
        base->calcFitness(&particle, false, false);

        const double fitness = particle.fitness(0);
        int totalRejections = task.rejectionCount(PSOTaskFoil::ConstraintType::MinThickness)
                            + task.rejectionCount(PSOTaskFoil::ConstraintType::MaxThickness)
                            + task.rejectionCount(PSOTaskFoil::ConstraintType::MaxCamber);

        const bool ok = particle.isConverged()
                     && !isPenalty(fitness)
                     && std::isfinite(fitness)
                     && totalRejections == 0;

        std::cout << "  Converged: " << (particle.isConverged() ? "true" : "false") << "\n";
        std::cout << "  Fitness: " << fitness << "\n";
        std::cout << "  Rejections: " << totalRejections << "\n";
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"Satisfied constraints pass", ok});
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
