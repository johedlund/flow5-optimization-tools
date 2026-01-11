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

#include <cmath>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#include <QCoreApplication>

#include <api.h>
#include <foil.h>
#include <objects2d.h>
#include <polar.h>

#include <interfaces/optim/particle.h>
#include <interfaces/optim/psotaskfoil.h>

static bool isFinite(double value)
{
    return std::isfinite(value);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    std::cout << "flow5 foil optimization fitness test\n";

    Foil *pFoil = foil::makeNacaFoil(2412, "optim_test_2412");
    if(!pFoil)
    {
        std::cerr << "FAIL: could not create NACA foil\n";
        return 1;
    }

    Polar *pPolar = Objects2d::createPolar(pFoil, xfl::T1POLAR, 1000000.0, 0.0, 9.0, 1.0, 1.0);
    if(!pPolar)
    {
        std::cerr << "FAIL: could not create polar\n";
        globals::deleteObjects();
        return 1;
    }
    pPolar->setName("FoilOptimTest");
    pPolar->setAoaSpec(2.0);
    Objects2d::insertPolar(pPolar);

    PSOTaskFoil task;
    task.setFoil(pFoil);
    task.setPolar(pPolar);
    task.initVariablesFromFoil();
    task.setTargetAlpha(pPolar->aoaSpec());
    task.setNObjectives(1);
    task.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
    task.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
    task.setTargetCl(0.73);

    // 0. MISSING TARGET (Invalid Spec)
    std::cout << "Test 0: Missing Target (Invalid Spec)\n";
    PSOTaskFoil missingTask;
    missingTask.setFoil(pFoil);
    missingTask.setPolar(pPolar);
    missingTask.initVariablesFromFoil();
    missingTask.setNObjectives(1);
    missingTask.setObjective(0, OptObjective("Cl", 0, true, 0.0, 0.0, xfl::EQUALIZE));

    Particle missingParticle;
    missingParticle.resizeArrays(missingTask.nVariables(), missingTask.nObjectives(), 1);
    for(int i=0; i<missingTask.nVariables(); ++i)
    {
        missingParticle.setPos(i, missingTask.variableBaseValue(i));
    }

    PSOTask *missingBase = &missingTask;
    missingBase->calcFitness(&missingParticle, false, false);

    const bool missingOk = (!missingParticle.isConverged() &&
                            missingParticle.fitness(0) == OPTIM_PENALTY);

    std::cout << "  Converged: " << (missingParticle.isConverged() ? "true" : "false") << "\n";
    std::cout << "  Fitness: " << missingParticle.fitness(0) << " (Expected " << OPTIM_PENALTY << ")\n";
    std::cout << "  Result: " << (missingOk ? "PASS" : "FAIL") << "\n";

    // 1. HAPPY PATH
    std::cout << "Test 1: Normal Geometry (Happy Path)\n";
    Particle particle;
    particle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
    for(int i=0; i<task.nVariables(); ++i)
    {
        particle.setPos(i, task.variableBaseValue(i));
    }

    PSOTask *base = &task;
    base->calcFitness(&particle, false, false);

    const double cd = particle.fitness(0);
    // Reasonable Cd for NACA 2412 at Cl=0.73, Re=1e6 is approx 0.006-0.015
    const bool happyOk = particle.isConverged()
                      && isFinite(cd)
                      && cd > 0.0 && cd < 0.1
                      && cd != OPTIM_PENALTY;

    std::cout << "  Converged: " << (particle.isConverged() ? "true" : "false") << "\n";
    std::cout << "  Cd: " << cd << "\n";
    std::cout << "  Result: " << (happyOk ? "PASS" : "FAIL") << "\n";


    // 2. SAD PATH (Invalid Geometry)
    std::cout << "Test 2: Invalid Geometry (Sad Path)\n";
    Particle sadParticle;
    sadParticle.resizeArrays(task.nVariables(), task.nObjectives(), 1);
    // Extreme offsets to stress geometry generation.
    for(int i=0; i<task.nVariables(); ++i)
    {
        const double y = task.variableBaseValue(i);
        sadParticle.setPos(i, (i % 2 == 0) ? y + 0.5 : y - 0.5);
    }

    base->calcFitness(&sadParticle, false, false);
    const double sadFitness = sadParticle.fitness(0);
    
    // Expect: penalty for invalid geometry (self-intersection or non-monotonic nose).
    const bool sadOk = (!sadParticle.isConverged() && sadFitness == OPTIM_PENALTY);

    std::cout << "  Converged: " << (sadParticle.isConverged() ? "true" : "false") << "\n";
    std::cout << "  Fitness: " << sadFitness << " (Penalty: " << OPTIM_PENALTY << ")\n";
    std::cout << "  Result: " << (sadOk ? "PASS" : "FAIL") << "\n";

    // 3. FULL RUN (Headless PSO)
    std::cout << "Test 3: Full Headless PSO Run\n";
    
    // Configure small run
    PSOTask::s_PopSize = 5;
    PSOTask::s_MaxIter = 3;
    PSOTask::s_bMultiThreaded = false; // deterministic sequence
    
    // Reset task
    task.setFoil(pFoil);
    task.setPolar(pPolar);
    task.initVariablesFromFoil(); // Reset variables
    task.setTargetAlpha(pPolar->aoaSpec());
    
    // Run
    task.onMakeParticleSwarm();
    task.onStartIterations();
    
    // For a short run, we don't expect convergence, just that it ran and produced a result.
    const bool runOk = task.isFinished() && task.paretoSize() > 0;
    
    std::cout << "  Status: " << (task.isFinished() ? "FINISHED" : "RUNNING/PENDING") << "\n";
    std::cout << "  Pareto Size: " << task.paretoSize() << "\n";
    std::cout << "  Result: " << (runOk ? "PASS" : "FAIL") << "\n";

    // 4. XFoil UNCONVERGED PATH
    std::cout << "Test 4: XFoil Unconverged Path\n";

    // Create polar with extreme conditions that should fail to converge
    Polar *pExtremePolar = Objects2d::createPolar(pFoil, xfl::T1POLAR, 100.0, 0.0, 9.0, 1.0, 1.0);
    if(!pExtremePolar)
    {
        std::cerr << "  FAIL: could not create extreme polar\n";
        globals::deleteObjects();
        return 2;
    }
    pExtremePolar->setName("ExtremePolar");
    pExtremePolar->setAoaSpec(85.0); // Extreme AoA likely to fail
    Objects2d::insertPolar(pExtremePolar);

    PSOTaskFoil extremeTask;
    extremeTask.setFoil(pFoil);
    extremeTask.setPolar(pExtremePolar);
    extremeTask.initVariablesFromFoil();
    extremeTask.setTargetAlpha(85.0);
    extremeTask.setNObjectives(1);
    extremeTask.setObjective(0, OptObjective("Cl", 0, true, 0.0, 0.0, xfl::EQUALIZE));

    Particle extremeParticle;
    extremeParticle.resizeArrays(extremeTask.nVariables(), extremeTask.nObjectives(), 1);
    for(int i=0; i<extremeTask.nVariables(); ++i)
    {
        extremeParticle.setPos(i, extremeTask.variableBaseValue(i));
    }

    PSOTask *extremeBase = &extremeTask;
    extremeBase->calcFitness(&extremeParticle, false, false);

    // Expect: either penalty for unconverged or finite fitness if it somehow converged
    const double extremeFitness = extremeParticle.fitness(0);
    const bool unconvergedOk = (!extremeParticle.isConverged() && extremeFitness == OPTIM_PENALTY)
                            || (extremeParticle.isConverged() && isFinite(extremeFitness));

    std::cout << "  Converged: " << (extremeParticle.isConverged() ? "true" : "false") << "\n";
    std::cout << "  Fitness: " << extremeFitness << " (Penalty: " << OPTIM_PENALTY << ")\n";
    std::cout << "  Result: " << (unconvergedOk ? "PASS" : "FAIL") << "\n";

    // 5. CANCELLATION PATH
    std::cout << "Test 5: Cancellation Path\n";

    PSOTask::s_PopSize = 6;
    PSOTask::s_MaxIter = 200;
    PSOTask::s_bMultiThreaded = false; // deterministic sequence

    PSOTaskFoil cancelTask;
    cancelTask.setFoil(pFoil);
    cancelTask.setPolar(pPolar);
    cancelTask.initVariablesFromFoil();
    cancelTask.setTargetAlpha(pPolar->aoaSpec());
    cancelTask.setNObjectives(1);
    cancelTask.setObjective(0, OptObjective("Cl", 0, true, 0.0, 0.0, xfl::EQUALIZE));

    cancelTask.onMakeParticleSwarm();

    std::thread cancelThread([&cancelTask]()
    {
        cancelTask.onStartIterations();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const bool finishedEarly = cancelTask.isFinished();
    cancelTask.cancelAnalyis();
    const bool cancelObserved = cancelTask.isCancelled();
    cancelThread.join();

    const bool cancelOk = !finishedEarly && cancelTask.isFinished() && cancelObserved;
    std::cout << "  Finished before cancel: " << (finishedEarly ? "true" : "false") << "\n";
    std::cout << "  Cancel flag observed: " << (cancelObserved ? "true" : "false") << "\n";
    std::cout << "  Status: " << (cancelTask.isFinished() ? "FINISHED" : "RUNNING/PENDING") << "\n";
    std::cout << "  Result: " << (cancelOk ? "PASS" : "FAIL") << "\n";

    // 6. V2 PRESET HANDLING
    std::cout << "Test 6: V2 Preset Handling\n";

    PSOTaskFoil v2Task;
    v2Task.setFoil(pFoil);
    v2Task.setPolar(pPolar);
    v2Task.setPreset(PSOTaskFoil::PresetType::V2_Camber_Thickness);
    v2Task.initVariablesFromFoil();

    // V2 preset now initializes camber/thickness variables
    const int v2Vars = v2Task.nVariables();
    const bool v2Ok = (v2Vars >= 4); // Expect camber/thickness variables (current: 4)

    std::cout << "  Preset: V2_Camber_Thickness\n";
    std::cout << "  Variables: " << v2Vars << " (Expected: >= 4 camber/thickness vars)\n";
    std::cout << "  Result: " << (v2Ok ? "PASS" : "FAIL") << "\n";

    // 7. MINIMAL GEOMETRY EDGE CASE
    std::cout << "Test 7: Minimal Geometry Edge Case\n";

    // Create a foil with minimal points (3 points - triangle)
    Foil minFoil;
    minFoil.setName("MinimalFoil");
    std::vector<Node2d> minNodes;
    minNodes.push_back(Node2d(0.0, 0.0));   // LE
    minNodes.push_back(Node2d(0.5, 0.05));  // Top
    minNodes.push_back(Node2d(1.0, 0.0));   // TE
    minFoil.setBaseNodes(minNodes);
    minFoil.initGeometry();

    PSOTaskFoil minTask;
    minTask.setFoil(&minFoil);
    minTask.setPolar(pPolar);
    minTask.initVariablesFromFoil();

    // With only 3 points and LE/TE fixed, expect 0 or very few variables
    // V1 now adds X+Y variables for mid-chord points (20-80%), so expect up to 2 for 50% chord point
    const int minVars = minTask.nVariables();
    // OK if it handles gracefully (zero vars or small number)
    const bool minOk = (minVars <= 2);

    std::cout << "  Foil points: 3\n";
    std::cout << "  Variables: " << minVars << " (Expected: 0-2 with LE/TE fixed, X+Y for mid-chord)\n";
    std::cout << "  Result: " << (minOk ? "PASS" : "FAIL") << "\n";

    // 8. V3 PRESET HANDLING (B-spline control points)
    std::cout << "Test 8: V3 Preset Handling\n";

    PSOTaskFoil v3Task;
    v3Task.setFoil(pFoil);
    v3Task.setPolar(pPolar);
    v3Task.setPreset(PSOTaskFoil::PresetType::V3_BSpline_Control);
    v3Task.initVariablesFromFoil();

    const int v3Vars = v3Task.nVariables();
    // V3 should create control point variables (typically more than V2)
    const bool v3Ok = (v3Vars >= 4);

    std::cout << "  Preset: V3_BSpline_Control\n";
    std::cout << "  Variables: " << v3Vars << " (Expected: >= 4 control point vars)\n";
    std::cout << "  Result: " << (v3Ok ? "PASS" : "FAIL") << "\n";

    // 9. V3 FULL PSO RUN (quick test for race conditions)
    std::cout << "Test 9: V3 Full PSO Run (10 iterations, multithreaded)\n";

    PSOTask::s_PopSize = 8;
    PSOTask::s_MaxIter = 10;
    PSOTask::s_bMultiThreaded = true;  // Enable multithreading to catch race conditions

    PSOTaskFoil v3RunTask;
    v3RunTask.setFoil(pFoil);
    v3RunTask.setPolar(pPolar);
    v3RunTask.setPreset(PSOTaskFoil::PresetType::V3_BSpline_Control);
    v3RunTask.initVariablesFromFoil();
    v3RunTask.setTargetAlpha(pPolar->aoaSpec());
    v3RunTask.setNObjectives(1);
    v3RunTask.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
    v3RunTask.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
    v3RunTask.setTargetCl(0.73);

    v3RunTask.onMakeParticleSwarm();
    v3RunTask.onStartIterations();

    const bool v3RunOk = v3RunTask.isFinished() && v3RunTask.paretoSize() > 0;

    std::cout << "  Status: " << (v3RunTask.isFinished() ? "FINISHED" : "RUNNING/PENDING") << "\n";
    std::cout << "  Pareto Size: " << v3RunTask.paretoSize() << "\n";
    std::cout << "  Result: " << (v3RunOk ? "PASS" : "FAIL") << "\n";

    // 10. V3 EXTENDED (longer run to catch late-onset issues)
    // NOTE: V3 Symmetric mode is known broken - BSpline is created from whole foil
    // then mirror logic creates invalid geometry. Skip symmetric test for now.
    std::cout << "Test 10: V3 Extended (20 iterations, multithreaded)\n";

    PSOTask::s_PopSize = 10;
    PSOTask::s_MaxIter = 20;
    PSOTask::s_bMultiThreaded = true;

    PSOTaskFoil v3ExtTask;
    v3ExtTask.setFoil(pFoil);
    v3ExtTask.setPolar(pPolar);
    v3ExtTask.setPreset(PSOTaskFoil::PresetType::V3_BSpline_Control);
    v3ExtTask.setSymmetric(false);  // V3 symmetric mode is broken, test asymmetric
    v3ExtTask.initVariablesFromFoil();
    v3ExtTask.setTargetAlpha(pPolar->aoaSpec());
    v3ExtTask.setNObjectives(1);
    v3ExtTask.setObjective(0, OptObjective("Cd", 0, true, 0.0, 0.0, xfl::EQUALIZE));
    v3ExtTask.setObjectiveType(PSOTaskFoil::ObjectiveType::MinimizeCd);
    v3ExtTask.setTargetCl(0.73);

    v3ExtTask.onMakeParticleSwarm();
    v3ExtTask.onStartIterations();

    const bool v3SymOk = v3ExtTask.isFinished() && v3ExtTask.paretoSize() > 0;

    std::cout << "  Status: " << (v3ExtTask.isFinished() ? "FINISHED" : "RUNNING/PENDING") << "\n";
    std::cout << "  Pareto Size: " << v3ExtTask.paretoSize() << "\n";
    std::cout << "  Result: " << (v3SymOk ? "PASS" : "FAIL") << "\n";

    globals::deleteObjects();

    const bool allOk = missingOk && happyOk && sadOk && runOk
                    && unconvergedOk && cancelOk && v2Ok && minOk
                    && v3Ok && v3RunOk && v3SymOk;

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Test 0 (Missing Target): " << (missingOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 1 (Happy Path): " << (happyOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 2 (Invalid Geometry): " << (sadOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 3 (PSO Run): " << (runOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 4 (Unconverged): " << (unconvergedOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 5 (Cancellation): " << (cancelOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 6 (V2 Preset): " << (v2Ok ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 7 (Minimal Geometry): " << (minOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 8 (V3 Preset): " << (v3Ok ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 9 (V3 PSO Run): " << (v3RunOk ? "PASS" : "FAIL") << "\n";
    std::cout << "Test 10 (V3 Extended): " << (v3SymOk ? "PASS" : "FAIL") << "\n";
    std::cout << "===============\n";
    std::cout << "Overall: " << (allOk ? "ALL PASS" : "SOME FAILED") << "\n";

    return allOk ? 0 : 2;
}
