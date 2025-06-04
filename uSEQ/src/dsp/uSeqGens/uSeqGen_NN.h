#ifndef USEQGEN_NN_H
#define USEQGEN_NN_H

#include "uSeqGen_Base.h"
#include "memlp/MLP.h"
#include "memlp/Dataset.hpp"


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

        nnInputs.resize(n_inputs+kBias);
        nnInputs[n_inputs] = 1.f; // bias
        nnOutputs.resize(n_outputs);
        for(size_t i=0; i < n_outputs; i++) {
            nnOutputs[i] = 0.f;
        }
        
        // Create dataset
        dataset = std::make_unique<Dataset>();


        addMessageHandler("rand", [this](float value) {
            mlp->DrawWeights();
        });        

        addMessageHandler("collect", [this](float value) {
            //store most recent input/output pair in training data
            dataset->Add(nnInputs, nnOutputs);
            println("Added data point");
            println(String(dataset->GetFeatures().size()));
        });        

        addMessageHandler("clear", [this](float value) {
            dataset->Clear();
        });        

        addMessageHandler("train", [this](float value) {
            size_t n_iterations = static_cast<size_t>(value);
            MLP<float>::training_pair_t ds(dataset->GetFeatures(), dataset->GetLabels());
            // Check and report on dataset size
            println("Feature size ");
            println(String(ds.first.size()));
            println(", label size ");
            println(String(ds.second.size()));
            if (!ds.first.size() || !ds.second.size()) {
                println("Empty dataset!");
                return;
            }
            println("Feature dim ");
            println(String(ds.first[0].size()));
            println(", label dim ");
            println(String(ds.second[0].size()));
            if (!ds.first[0].size() || !ds.second[0].size()) {
                println("Empty dataset dimensions!");
                return;
            }

            // Training loop
            println("Training for max ");
            println(String(n_iterations));
            println(" iterations...");
            // float loss = mlp->Train(ds,
            //         1.,
            //         n_iterations,
            //         0.0001,
            //         false);
            // println("Trained, loss = ");
            // println(String(loss));

        });        

        addMessageHandler("postdata", [this](float value) {
        });        
    }

protected:


    void __force_inline Process_(DSPatch::SignalBus& inputs, DSPatch::SignalBus& outputs) override
    {
        if (divCount == 0) {
            for(size_t i=0; i < n_inputs; i++) {
                nnInputs[i] = GET_INPUT_SAFE(inputs, float, i, 0.0);;
            }
            mlp->GetOutput(nnInputs, &nnOutputs);
        }
        for(size_t i=0; i < n_outputs; i++) {
            outputs.SetValue(i, nnOutputs[i]);
        }

        divCount++;
        if (divCount >= divisor) {
            divCount = 0;
        }
    }

    std::unique_ptr<MLP<float>> mlp;
    std::unique_ptr<Dataset> dataset;



private:
    size_t divisor=10;
    size_t divCount=0;
    std::vector<float> nnOutputs, nnInputs;
    size_t n_inputs = 2;
    size_t n_outputs = 8;

};

#endif 
