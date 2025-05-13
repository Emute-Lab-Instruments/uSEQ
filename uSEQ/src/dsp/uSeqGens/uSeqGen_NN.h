#ifndef USEQGEN_NN_H
#define USEQGEN_NN_H

#include "uSeqGen_Base.h"
#include "memlp/MLP.h"

class uSeqGen_NN final : public uSeqGen_Base
{
public:
    uSeqGen_NN(queue_t *q, size_t key) : uSeqGen_Base(q, key)
    {
        const unsigned int kBias = 1;
        const std::vector<ACTIVATION_FUNCTIONS> layers_activfuncs = {
            RELU, RELU, RELU, SIGMOID
        };
        const bool use_constant_weight_init = false;
        const float constant_weight_init = 0;
        size_t n_inputs = 2;
        size_t n_outputs = 1;
        // Layer size definitions
        const std::vector<size_t> layers_nodes = {
            n_inputs + kBias,
            10, 10, 14,
            n_outputs
        };

        // Create MLP
        mlp = std::make_unique<MLP<float>>(
            layers_nodes,
            layers_activfuncs,
            loss::LOSS_MSE,
            use_constant_weight_init,
            constant_weight_init
        );


        SetInputCount_(n_inputs);
        SetOutputCount_(n_outputs);

        // addMessageHandler("n", [this](float value) {
        //     n = static_cast<size_t>(value);
        // });        
    }

protected:


    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        outputs.SetValue(0, 0.49f);
    }

    std::unique_ptr<MLP<float>> mlp;


private:
};

#endif 
