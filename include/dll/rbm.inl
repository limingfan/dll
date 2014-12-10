//=======================================================================
// Copyright (c) 2014 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#ifndef DLL_RBM_INL
#define DLL_RBM_INL

#include "cpp_utils/assert.hpp"             //Assertions
#include "cpp_utils/stop_watch.hpp"         //Performance counter

#include "etl/etl.hpp"

#include "standard_rbm.hpp"
#include "checks.hpp"

namespace dll {

/*!
 * \brief Standard version of Restricted Boltzmann Machine
 *
 * This follows the definition of a RBM by Geoffrey Hinton.
 */
template<typename Desc>
struct rbm final : public standard_rbm<rbm<Desc>, Desc> {
    using desc = Desc;
    using weight = typename desc::weight;

    static constexpr const std::size_t num_visible = desc::num_visible;
    static constexpr const std::size_t num_hidden = desc::num_hidden;

    static constexpr const unit_type visible_unit = desc::visible_unit;
    static constexpr const unit_type hidden_unit = desc::hidden_unit;

    //Weights and biases
    etl::fast_matrix<weight, num_visible, num_hidden> w;    //!< Weights
    etl::fast_vector<weight, num_hidden> b;                 //!< Hidden biases
    etl::fast_vector<weight, num_visible> c;                //!< Visible biases

    //Reconstruction data
    etl::fast_vector<weight, num_visible> v1; //!< State of the visible units

    etl::fast_vector<weight, num_hidden> h1_a; //!< Activation probabilities of hidden units after first CD-step
    etl::fast_vector<weight, num_hidden> h1_s; //!< Sampled value of hidden units after first CD-step

    etl::fast_vector<weight, num_visible> v2_a; //!< Activation probabilities of visible units after first CD-step
    etl::fast_vector<weight, num_visible> v2_s; //!< Sampled value of visible units after first CD-step

    etl::fast_vector<weight, num_hidden> h2_a; //!< Activation probabilities of hidden units after last CD-step
    etl::fast_vector<weight, num_hidden> h2_s; //!< Sampled value of hidden units after last CD-step

    //No copying
    rbm(const rbm& rbm) = delete;
    rbm& operator=(const rbm& rbm) = delete;

    //No moving
    rbm(rbm&& rbm) = delete;
    rbm& operator=(rbm&& rbm) = delete;

    /*!
     * \brief Initialize a RBM with basic weights.
     *
     * The weights are initialized from a normal distribution of
     * zero-mean and 0.1 variance.
     */
    rbm() : standard_rbm<rbm<Desc>, Desc>(), b(0.0), c(0.0) {
        //Initialize the weights with a zero-mean and unit variance Gaussian distribution
        w = etl::normal_generator<weight>() * 0.1;
    }

    static constexpr std::size_t input_size() noexcept {
        return num_visible;
    }

    static constexpr std::size_t output_size() noexcept {
        return num_hidden;
    }

    void display() const {
        std::cout << "RBM: " << num_visible << " -> " << num_hidden << std::endl;
    }

    template<bool P = true, bool S = true, typename H1, typename H2, typename V>
    void activate_hidden(H1&& h_a, H2&& h_s, const V& v_a, const V& v_s) const {
        activate_hidden<P, S>(std::forward<H1>(h_a), std::forward<H2>(h_s), v_a, v_s, b, w);
    }

    template<bool P = true, bool S = true, typename H1, typename H2, typename V, typename T>
    void activate_hidden(H1&& h_a, H2&& h_s, const V& v_a, const V& v_s, T&& t) const {
        activate_hidden<P, S>(std::forward<H1>(h_a), std::forward<H2>(h_s), v_a, v_s, b, w, std::forward<T>(t));
    }

    template<bool P = true, bool S = true, typename H1, typename H2, typename V, typename B, typename W>
    static void activate_hidden(H1&& h_a, H2&& h_s, const V& v_a, const V& v_s, const B& b, const W& w){
        static etl::fast_matrix<weight, 1, num_hidden> t;

        activate_hidden<P, S>(std::forward<H1>(h_a), std::forward<H2>(h_s), v_a, v_s, b, w, t);
    }

    template<bool P = true, bool S = true, typename H1, typename H2, typename V, typename B, typename W, typename T>
    static void activate_hidden(H1&& h_a, H2&& h_s, const V& v_a, const V&, const B& b, const W& w, T&& t){
        using namespace etl;

        //Compute activation probabilities
        if(P){
            if(hidden_unit == unit_type::BINARY){
                h_a = sigmoid(b + auto_vmmul(v_a, w, t));
            } else if(hidden_unit == unit_type::RELU){
                h_a = max(b + auto_vmmul(v_a, w, t), 0.0);
            } else if(hidden_unit == unit_type::RELU6){
                h_a = min(max(b + auto_vmmul(v_a, w, t), 0.0), 6.0);
            } else if(hidden_unit == unit_type::RELU1){
                h_a = min(max(b + auto_vmmul(v_a, w, t), 0.0), 1.0);
            } else if(hidden_unit == unit_type::SOFTMAX){
                h_a = softmax(b + auto_vmmul(v_a, w, t));
            }

            //Compute sampled values directly
            if(S){
                if(hidden_unit == unit_type::BINARY){
                    h_s = bernoulli(h_a);
                } else if(hidden_unit == unit_type::RELU){
                    h_s = logistic_noise(h_a); //TODO This is probably wrong
                } else if(hidden_unit == unit_type::RELU6){
                    h_s = ranged_noise(h_a, 6.0); //TODO This is probably wrong
                } else if(hidden_unit == unit_type::RELU1){
                    h_s = ranged_noise(h_a, 1.0); //TODO This is probably wrong
                } else if(hidden_unit == unit_type::SOFTMAX){
                    h_s = one_if_max(h_a);
                }
            }
        }
        //Compute sampled values
        else if(S){
            if(hidden_unit == unit_type::BINARY){
                h_s = bernoulli(sigmoid(b + auto_vmmul(v_a, w, t)));
            } else if(hidden_unit == unit_type::RELU){
                h_s = logistic_noise(max(b + auto_vmmul(v_a, w, t), 0.0)); //TODO This is probably wrong
            } else if(hidden_unit == unit_type::RELU6){
                h_s = ranged_noise(min(max(b + auto_vmmul(v_a, w, t), 0.0), 6.0), 6.0); //TODO This is probably wrong
            } else if(hidden_unit == unit_type::RELU1){
                h_s = ranged_noise(min(max(b + auto_vmmul(v_a, w, t), 0.0), 1.0), 1.0); //TODO This is probably wrong
            } else if(hidden_unit == unit_type::SOFTMAX){
                h_s = one_if_max(softmax(b + auto_vmmul(v_a, w, t)));
            }
        }

        nan_check_deep(h_a);
        nan_check_deep(h_s);
    }

    template<bool P = true, bool S = true, typename H, typename V>
    void activate_visible(const H& h_a, const H& h_s, V&& v_a, V&& v_s) const {
        using namespace etl;

        static fast_matrix<weight, num_visible, 1> t;

        activate_visible<P, S>(h_a, h_s, std::forward<V>(v_a), std::forward<V>(v_s), c, w, t);
    }

    template<bool P = true, bool S = true, typename H, typename V, typename T>
    void activate_visible(const H& h_a, const H& h_s, V&& v_a, V&& v_s, T&& t) const {
        activate_visible<P, S>(h_a, h_s, std::forward<V>(v_a), std::forward<V>(v_s), c, w, std::forward<T>(t));
    }

    template<bool P = true, bool S = true, typename H, typename V, typename C, typename W, typename T>
    static void activate_visible(const H&, const H& h_s, V&& v_a, V&& v_s, const C& c, const W& w, T&& t){
        using namespace etl;

        if(P){
            if(visible_unit == unit_type::BINARY){
                v_a = sigmoid(c + auto_vmmul(w, h_s, t));
            } else if(visible_unit == unit_type::GAUSSIAN){
                v_a = c + auto_vmmul(w, h_s, t);
            } else if(visible_unit == unit_type::RELU){
                v_a = max(c + auto_vmmul(w, h_s, t), 0.0);
            }
        }

        if(S){
            if(visible_unit == unit_type::BINARY){
                v_s = bernoulli(sigmoid(c + auto_vmmul(w, h_s, t)));
            } else if(visible_unit == unit_type::GAUSSIAN){
                v_s = c + auto_vmmul(w, h_s, t);
            } else if(visible_unit == unit_type::RELU){
                v_s = logistic_noise(max(c + auto_vmmul(w, h_s, t), 0.0));
            }
        }

        nan_check_deep(v_a);
        nan_check_deep(v_s);
    }

    template<typename Sample, typename Output>
    void activation_probabilities(const Sample& item_data, Output& result){
        etl::dyn_vector<weight> item(item_data);

        static etl::dyn_vector<weight> next_s(num_hidden);

        activate_hidden(result, next_s, item, item);
    }

    template<typename Sample>
    etl::dyn_vector<weight> activation_probabilities(const Sample& item_data){
        etl::dyn_vector<weight> result(output_size());

        activation_probabilities(item_data, result);

        return result;
    }
};

} //end of dll namespace

#endif
