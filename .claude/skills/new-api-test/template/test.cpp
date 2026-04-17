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
 * __DESCRIPTION__
 */

#include <cmath>
#include <iostream>
#include <vector>

#include <QCoreApplication>

#include <api.h>
#include <objects2d.h>

struct TestResult
{
    const char *name;
    bool passed;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    std::cout << "flow5 __DESCRIPTION__\n\n";

    std::vector<TestResult> results;

    // =========================================================
    // Test 1: placeholder — replace with real assertions
    // =========================================================
    std::cout << "Test 1: placeholder\n";
    {
        bool ok = true;
        std::cout << "  Result: " << (ok ? "PASS" : "FAIL") << "\n\n";
        results.push_back({"placeholder", ok});
    }

    // =========================================================
    // Summary
    // =========================================================
    std::cout << "=== SUMMARY ===\n";
    bool allPassed = true;
    for (auto const &r : results)
    {
        std::cout << "  " << r.name << ": " << (r.passed ? "PASS" : "FAIL") << "\n";
        if (!r.passed) allPassed = false;
    }
    std::cout << "===============\n";
    std::cout << "Overall: " << (allPassed ? "ALL PASS" : "SOME FAILED") << "\n";

    globals::deleteObjects();
    return allPassed ? 0 : 2;
}
