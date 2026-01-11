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

#pragma once

#include <interfaces/optim/psotask.h>

#include <api/bspline.h>
#include <api/vector2d.h>
#include <node2d.h>

#include <atomic>
#include <array>

class Foil;
class Polar;
class PlaneXfl;

class PSOTaskFoil : public PSOTask
{
    public:
        // Constraint types for rejection tracking
        enum class ConstraintType
        {
            // Geometry validation (from isFoilGeometryValid)
            GeomTEGap,
            GeomChord,
            GeomMonotonicLE,
            GeomSelfIntersection,
            GeomPositiveThicknessLE,
            GeomLERadius,
            GeomLECurvature,
            GeomLETurnAngle,
            // Geometric constraints
            MinThickness,
            MaxThickness,
            MinTEThickness,
            MinLERadius,
            MaxWiggliness,
            MinSectionModulus,
            MinCamber,
            MaxCamber,
            MinXCamber,
            MaxXCamber,
            MinXThickness,
            MaxXThickness,
            MinArea,
            MinThickAt80,
            MinThickAt90,
            // Aerodynamic constraints
            MinCl,
            MaxCl,
            MinCd,
            MaxCd,
            MinCm,
            MaxCm,
            MinLD,
            MaxDCpDxAt10,
            MaxDCpDxAt25,
            MaxDCpDxAt50,
            MaxDCpDxAt75,
            // XFoil convergence failure
            XFoilFailed,
            COUNT  // Must be last - total count of constraint types
        };

        static constexpr int ConstraintTypeCount = static_cast<int>(ConstraintType::COUNT);

        enum class OptimizationMode
        {
            ModeA,  // 2D only (fixed AoA)
            ModeB   // 3D coupled (induced AoA correction)
        };

        enum class TargetMode
        {
            Polar,
            Alpha,
            Cl
        };

        enum class PresetType
        {
            V1_Y_Only,
            V2_Camber_Thickness,
            V3_BSpline_Control  // B-spline control points (approximating, smoother)
        };

        enum class ObjectiveType
        {
            MinimizeCd,
            MaximizeLD,
            MaximizeCl,
            MinimizeCm,
            TargetCl,
            TargetCm,
            MaximizePowerFactor, // Cl^1.5 / Cd
            MaximizeEnduranceFactor // Cl^3 / Cd^2
        };

        // Multi-objective specification for weighted sum optimization
        struct ObjectiveSpec
        {
            ObjectiveType type{ObjectiveType::MinimizeCd};
            TargetMode targetMode{TargetMode::Alpha};
            double targetValue{0.0};      // Alpha (degrees) or Cl
            double weight{1.0};           // Relative weight (positive)
            double reynolds{1.0e6};       // Per-objective Reynolds number
            double normFactor{1.0};       // Computed from baseline foil
            bool enabled{true};
        };

        struct ConstraintVal
        {
            double value{0.0};
            bool enabled{false};
        };

        struct Constraints
        {
            // Geometric
            ConstraintVal minThickness;
            ConstraintVal maxThickness;
            ConstraintVal minLERadius;
            ConstraintVal minTEThickness;
            ConstraintVal maxWiggliness;
            ConstraintVal minSectionModulus;

            ConstraintVal minCamber;
            ConstraintVal maxCamber;
            ConstraintVal minXCamber;
            ConstraintVal maxXCamber;
            ConstraintVal minXThickness;
            ConstraintVal maxXThickness;
            ConstraintVal minArea; // Cross-sectional area

            // Position-based thickness constraints (prevents flat trailing edges)
            ConstraintVal minThickAt80; // Min thickness at 80% chord
            ConstraintVal minThickAt90; // Min thickness at 90% chord

            // Aerodynamic
            ConstraintVal minCl;
            ConstraintVal maxCl;
            ConstraintVal minCd;
            ConstraintVal maxCd;
            ConstraintVal minCm;
            ConstraintVal maxCm;
            ConstraintVal minLD;

            // Pressure gradient constraints (dCp/dx on upper surface)
            // Limits adverse pressure gradient to prevent flow separation
            ConstraintVal maxDCpDxAt10; // Max dCp/dx at 10% chord
            ConstraintVal maxDCpDxAt25; // Max dCp/dx at 25% chord
            ConstraintVal maxDCpDxAt50; // Max dCp/dx at 50% chord
            ConstraintVal maxDCpDxAt75; // Max dCp/dx at 75% chord

            bool enabled{false};
        };

        PSOTaskFoil();
        ~PSOTaskFoil() override;

        void setFoil(Foil *pFoil);
        void setPolar(Polar *pPolar);
        void setConstraints(Constraints const &c) {m_Constraints = c;}
        void setOptimizationPoints(int n) {m_OptimizationPoints = n;}
        void setBoundsScale(double scale) {m_BoundsScale = scale;}
        void setXBoundsScale(double scale) {m_XBoundsScale = scale;}
        void setSymmetric(bool bSym) {m_bSymmetric = bSym;}
        bool isSymmetric() const {return m_bSymmetric;}

        // X movement chord range (V1 preset)
        void setXMoveChordRange(double minChord, double maxChord) {m_XMoveMinChord = minChord; m_XMoveMaxChord = maxChord;}
        double xMoveMinChord() const {return m_XMoveMinChord;}
        double xMoveMaxChord() const {return m_XMoveMaxChord;}

        void initVariablesFromFoil(double yDelta=0.0);
        void setPreset(PresetType preset) {m_Preset = preset;}
        void setObjectiveType(ObjectiveType type) {m_ObjectiveType = type;}
        void setTargetAlpha(double alpha);
        void setTargetCl(double cl);
        
        void setReynolds(double re) {m_Reynolds = re;}
        void setMach(double ma) {m_Mach = ma;}
        void setNCrit(double nc) {m_NCrit = nc;}

        // Mode B (3D coupling)
        void setOptimizationMode(OptimizationMode mode) {m_OptMode = mode;}
        OptimizationMode optimizationMode() const {return m_OptMode;}
        void setPlane3D(PlaneXfl *pPlane, int wingIndex, int sectionIndex);
        void setInducedAlpha(double alpha) {m_InducedAlpha = alpha;}
        double inducedAlpha() const {return m_InducedAlpha;}
        PlaneXfl* plane3D() const {return m_pPlane3D;}
        int wingIndex() const {return m_WingIndex;}
        int sectionIndex() const {return m_SectionIndex;}

        void clearTargetOverride() {m_TargetMode = TargetMode::Polar;}
        TargetMode targetMode() const {return m_TargetMode;}
        int variableBaseIndex(int varIndex) const;
        double variableBaseY(int varIndex) const;
        double variableBaseValue(int varIndex) const;  // Returns X or Y based on m_VarIsX
        bool variableIsX(int varIndex) const;

        Foil* createOptimizedFoil(const Particle &p) const;

        // Visualization data for optimization preview
        PresetType preset() const {return m_Preset;}
        void getOptimMarkers(std::vector<std::pair<double, double>> &ctrlPts,
                             std::vector<std::tuple<double, double, double>> &bounds) const;
        void getCurrentMarkers(Particle const &p, std::vector<std::pair<double, double>> &ctrlPts) const;

        // Multi-objective weighted sum API
        void clearObjectiveSpecs() {m_ObjectiveSpecs.clear();}
        void addObjectiveSpec(const ObjectiveSpec &spec) {m_ObjectiveSpecs.push_back(spec);}
        void setObjectiveSpecs(const std::vector<ObjectiveSpec> &specs) {m_ObjectiveSpecs = specs;}
        int nObjectiveSpecs() const {return static_cast<int>(m_ObjectiveSpecs.size());}
        const ObjectiveSpec& objectiveSpec(int i) const {return m_ObjectiveSpecs.at(i);}
        void computeNormFactors();  // Evaluate baseline foil to normalize objectives

        // Constraint rejection tracking API
        void resetRejectionCounts() const;
        int rejectionCount(ConstraintType type) const;
        int totalEvaluations() const;
        std::array<int, ConstraintTypeCount> allRejectionCounts() const;
        static QString constraintTypeName(ConstraintType type);

    private:
        void incrementRejection(ConstraintType type) const;
        void calcFitness(Particle *pParticle, bool bLong=false, bool bTrace=false) const override;
        bool resolveTarget(bool &useAlpha, double &value) const;
        double evaluateObjectiveSpec(const Foil &workFoil, const ObjectiveSpec &spec, bool &success) const;

        Foil *m_pFoil{nullptr};
        Polar *m_pPolar{nullptr};
        Constraints m_Constraints;
        TargetMode m_TargetMode{TargetMode::Polar};
        ObjectiveType m_ObjectiveType{ObjectiveType::MaximizeLD};
        PresetType m_Preset{PresetType::V1_Y_Only};
        double m_TargetValue{0.0};

        // Multi-objective weighted sum
        std::vector<ObjectiveSpec> m_ObjectiveSpecs;
        double m_Reynolds{1.0e6};
        double m_Mach{0.0};
        double m_NCrit{9.0};

        // Mode B (3D coupling)
        OptimizationMode m_OptMode{OptimizationMode::ModeA};
        PlaneXfl *m_pPlane3D{nullptr};  // Not owned - just a reference
        int m_WingIndex{0};
        int m_SectionIndex{0};
        double m_InducedAlpha{0.0};
        int m_OptimizationPoints{0};
        double m_BoundsScale{1.0};
        double m_XBoundsScale{2.0};   // X bounds multiplier (relative to Y bounds)
        bool m_bSymmetric{false};

        // LE protection: blend optimized shape back toward original near LE
        double m_LEBlendChord{0.05};      // Blend region: 0-5% chord transitions from original to optimized

        // V1 X movement chord range (control points outside this range only move in Y)
        double m_XMoveMinChord{0.20};     // Start X movement at 20% chord
        double m_XMoveMaxChord{0.80};     // End X movement at 80% chord

        // V3 B-spline X-movement (kept for V3 compatibility, not used in V1)
        int m_LEXPoints{2};               // Number of LE-adjacent control points with X movement
        double m_LEXBoundsScale{0.2};     // X bounds = Y bounds * this factor

        std::vector<Node2d> m_OptimBaseNodes;
        std::vector<int> m_OptimBaseIndex;
        std::vector<int> m_VarToBase;
        std::vector<bool> m_VarIsX;       // true if variable is X coordinate, false for Y
        std::vector<bool> m_VarIsTop;     // true if variable is on top surface, false for bottom

        // LE tangent for split spline approach (cached from original foil)
        Vector2d m_BaseLETangent{0.0, 1.0};  // Default: vertical (perpendicular to chord)
        int m_LEOptimIndex{0};               // Index of LE in m_OptimBaseNodes

        // Preset V2 members
        double m_BaseMaxCamber{0.0};
        double m_BaseMaxThickness{0.0};
        double m_BaseXCamber{0.0};
        double m_BaseXThickness{0.0};
        double m_BaseLERadius{0.0};
        double m_BaseMaxLECurvature{0.0};
        double m_BaseMaxLETurnAngle{0.0};
        bool m_BaseHasMonotonicLE{false};
        bool m_BaseHasPositiveThicknessLE{false};
        bool m_BaseHasSelfIntersection{false};

        // Preset V3 (B-spline) members
        BSpline m_BaseBSpline;           // B-spline approximation of the original foil
        int m_BSplineDegree{3};          // Cubic B-spline (degree 3)
        int m_BSplineCtrlPts{0};         // Number of control points
        int m_BSplineOutputPts{200};     // Output resolution
        int m_BSplineLECtrlIndex{0};     // Index of LE control point (fixed)

        // Constraint rejection tracking (mutable for thread-safe updates from const calcFitness)
        mutable std::array<std::atomic<int>, ConstraintTypeCount> m_RejectionCounts{};
        mutable std::atomic<int> m_TotalEvaluations{0};
};
