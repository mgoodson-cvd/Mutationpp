#include "AutoRegistration.h"
#include "Errors.h"
#include "Thermodynamics.h"
#include "Transport.h"

#include "GSIRateLaw.h"

using namespace Eigen;

using namespace Mutation::Utilities;
using namespace Mutation::Utilities::Config;

namespace Mutation {
    namespace GasSurfaceInteraction {

class GSIRateLawGammaConst : public GSIRateLaw
{
public:
    GSIRateLawGammaConst(ARGS args)
        : GSIRateLaw (args),
          mv_react(args.s_reactants)
    {
        assert(args.s_node_rate_law.tag() == "gamma_const");

        int l_diff_reac = 1;
        for (int i_reac = 0; i_reac < mv_react.size() - 1; ++i_reac) {
            if (mv_react[i_reac] != mv_react[i_reac + 1]) l_diff_reac++;
        }
    
        std::vector<std::string> tokens;
        String::tokenize(
            args.s_node_rate_law.text(), tokens, ":, ");
        int index_ref = -2;
        for (int i = 0; i < tokens.size(); i+=2)
        {
            int index = m_thermo.speciesIndex(tokens[i]);
            if(index_ref > index){
                mv_gamma.insert(mv_gamma.begin(), atof(tokens[i+1].c_str()));
            } else {
                mv_gamma.insert(mv_gamma.end(), atof(tokens[i+1].c_str()));
            }
            index_ref = index;
        }
    
        if (l_diff_reac != mv_gamma.size()) {
            throw LogicError()
            << "Number of gammas should be equal to the number of "
               "different reactants.";
        }

        m_idx_react = -1;
        m_idx_sp = -1;
        m_stoich_coef = -1;

        mv_imp_flux_out.resize(mv_gamma.size());
        mv_imp_flux_per_stoich_coef.resize(mv_gamma.size());

    }

//==============================================================================

    ~GSIRateLawGammaConst(){}

//==============================================================================

    double forwardReactionRateCoefficient(
        const VectorXd& v_rhoi, const VectorXd& v_Twall) const
    {
        m_idx_react = 0;
        for(int i_g = 0; i_g < mv_gamma.size(); i_g++) {
            getSpeciesIndexandStoichiometricCoefficient(
                m_idx_react, m_idx_sp, m_stoich_coef);
            mv_imp_flux_out[i_g] = computeWallImpingingMassFlux(
                m_idx_sp, v_rhoi, v_Twall);
    
            mv_imp_flux_per_stoich_coef[i_g] =
                mv_imp_flux_out[i_g]/m_stoich_coef;
            mv_imp_flux_out[i_g] =
                mv_imp_flux_per_stoich_coef[i_g]*mv_gamma[i_g];
    
            m_idx_react += m_stoich_coef;
        }
    
        return getLimitingImpingingMassFlux();
    }

private:
    mutable int m_idx_react;
    mutable int m_idx_sp;
    mutable int m_stoich_coef;

    std::vector<double> mv_gamma;

    mutable std::vector<double> mv_imp_flux_out;
    mutable std::vector<double> mv_imp_flux_per_stoich_coef;

    const std::vector<int>& mv_react;

//==============================================================================

    inline void getSpeciesIndexandStoichiometricCoefficient(
        int idx_react, int& idx_sp, int& stoich_coef) const
    {
        idx_sp = mv_react[idx_react];
        stoich_coef = 1;
        idx_react++;
        
        while(idx_react < mv_react.size()) {
            if (idx_sp != mv_react[idx_react]) {
                break;
            } 
            stoich_coef++;
            idx_react++;
        }
    }

//==============================================================================

    inline double computeWallImpingingMassFlux(
        const int& idx_sp,
        const VectorXd& v_rhoi,
        const VectorXd& v_Twall) const
    {
    	const int set_state_with_rhoi_T = 1;
    	m_thermo.setState(
    	    v_rhoi.data(), v_Twall.data(), set_state_with_rhoi_T);
    	double m_sp_thermal_speed = m_transport.speciesThermalSpeed(idx_sp);

        return m_sp_thermal_speed/4.
        		* v_rhoi(idx_sp) / m_thermo.speciesMw(idx_sp);
    }

//==============================================================================

    inline double getLimitingImpingingMassFlux() const
    {
        return mv_imp_flux_out[min_element(
                   mv_imp_flux_per_stoich_coef.begin(),
                   mv_imp_flux_per_stoich_coef.end()) -
                   mv_imp_flux_per_stoich_coef.begin()];
    }
};

ObjectProvider<
    GSIRateLawGammaConst, GSIRateLaw>
    gsi_rate_law_gamma_const("gamma_const");

    } // namespace GasSurfaceInteraction
} // namespace Mutation
