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

#include <node2d.h>

class Foil;
class Polar;

class PSOTaskFoil : public PSOTask
{
    public:
        enum class TargetMode
        {
            Polar,
            Alpha,
            Cl
        };

        enum class PresetType
        {
            V1_Y_Only,
            V2_Camber_Thickness
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

            // Aerodynamic
            ConstraintVal minCl;
            ConstraintVal maxCl;
            ConstraintVal minCd;
            ConstraintVal maxCd;
            ConstraintVal minCm;
            ConstraintVal maxCm;
            ConstraintVal minLD;
            
            bool enabled{false};
        };

        PSOTaskFoil();
        ~PSOTaskFoil() override;

        void setFoil(Foil *pFoil);
        void setPolar(Polar *pPolar);
        void setConstraints(Constraints const &c) {m_Constraints = c;}
        void setOptimizationPoints(int n) {m_OptimizationPoints = n;}
        void setBoundsScale(double scale) {m_BoundsScale = scale;}

        void initVariablesFromFoil(double yDelta=0.0);
        void setPreset(PresetType preset) {m_Preset = preset;}
        void setObjectiveType(ObjectiveType type) {m_ObjectiveType = type;}
        void setTargetAlpha(double alpha);
        void setTargetCl(double cl);
        
        void setReynolds(double re) {m_Reynolds = re;}
        void setMach(double ma) {m_Mach = ma;}
        void setNCrit(double nc) {m_NCrit = nc;}

        void clearTargetOverride() {m_TargetMode = TargetMode::Polar;}
        TargetMode targetMode() const {return m_TargetMode;}
        int variableBaseIndex(int varIndex) const;
        double variableBaseY(int varIndex) const;

        Foil* createOptimizedFoil(const Particle &p) const;

    private:
        void calcFitness(Particle *pParticle, bool bLong=false, bool bTrace=false) const override;
        bool resolveTarget(bool &useAlpha, double &value) const;

        Foil *m_pFoil{nullptr};
        Polar *m_pPolar{nullptr};
        Constraints m_Constraints;
        TargetMode m_TargetMode{TargetMode::Polar};
        ObjectiveType m_ObjectiveType{ObjectiveType::MaximizeLD};
        PresetType m_Preset{PresetType::V1_Y_Only};
        double m_TargetValue{0.0};
        double m_Reynolds{1.0e6};
        double m_Mach{0.0};
        double m_NCrit{9.0};
        int m_OptimizationPoints{0};
        double m_BoundsScale{1.0};
        std::vector<Node2d> m_OptimBaseNodes;
        std::vector<int> m_OptimBaseIndex;
        std::vector<int> m_VarToBase;

        // Preset V2 members
        double m_BaseMaxCamber{0.0};
        double m_BaseMaxThickness{0.0};
        double m_BaseXCamber{0.0};
        double m_BaseXThickness{0.0};
};
