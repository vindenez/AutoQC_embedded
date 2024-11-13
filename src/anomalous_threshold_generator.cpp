#include "anomalous_threshold_generator.hpp"
#include "matrix_utils.hpp"
#include "activation_functions.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include "config.hpp"
#include <chrono>

AnomalousThresholdGenerator::AnomalousThresholdGenerator(int lstm_layer, int lstm_unit, int lookback_len, int prediction_len)
    : lookback_len(lookback_len),
      prediction_len(prediction_len),
      generator(lookback_len, lstm_unit, prediction_len, lstm_layer, lookback_len) {
}

void AnomalousThresholdGenerator::train(int num_epochs, float learning_rate, const std::vector<float>& data_to_learn) {
    auto [x, y] = sliding_windows(data_to_learn, lookback_len, prediction_len);
    
    // Calculate global statistics for normalization (like Python)
    float mean_global = 0.0f;
    float std_global = 0.0f;
    int total_values = 0;
    
    // First pass: calculate mean
    for (const auto& seq : x) {
        for (float val : seq) {
            mean_global += val;
            total_values++;
        }
    }
    mean_global /= total_values;
    
    // Second pass: calculate std
    for (const auto& seq : x) {
        for (float val : seq) {
            float diff = val - mean_global;
            std_global += diff * diff;
        }
    }
    std_global = std::sqrt(std_global / total_values);
    
    // Normalize data
    for (auto& seq : x) {
        for (float& val : seq) {
            val = (val - mean_global) / (std_global + 1e-10f);
        }
    }
    for (auto& seq : y) {
        for (float& val : seq) {
            val = (val - mean_global) / (std_global + 1e-10f);
        }
    }
    
    generator.train();
    generator.init_adam_optimizer(learning_rate);

    std::cout << "\nGenerator training for " << num_epochs << " epochs..." << std::endl;
    
    for (int epoch = 0; epoch < num_epochs; ++epoch) {
        float epoch_loss = 0.0f;
        
        for (size_t i = 0; i < x.size(); ++i) {
            generator.zero_grad();
            
            std::vector<std::vector<std::vector<float>>> reshaped_input(1);
            reshaped_input[0].push_back(x[i]);
            
            auto outputs = generator.forward(reshaped_input);
            std::vector<float> target = y[i];
            float loss = compute_mse_loss(outputs, target);
            
            generator.backward(target, "MSE");
            generator.update_parameters_adam(learning_rate);
            
            epoch_loss += loss;
        }
        
        if (epoch % 100 == 0) {
            std::cout << "Generator Training Epoch " << epoch << "/" << num_epochs 
                     << " Loss: " << std::scientific << epoch_loss/x.size() << std::endl;
        }
    }
    generator.eval();
}

float AnomalousThresholdGenerator::update(int epoch_update, float lr_update, 
                                        const std::vector<float>& past_errors, 
                                        float recent_error) {
    // First get initial prediction in eval mode
    generator.eval();
    std::vector<std::vector<std::vector<float>>> eval_input(1);
    eval_input[0].push_back(past_errors);
    auto initial_threshold = generator.forward(eval_input);
    
    // Then switch to training mode
    generator.train();
    generator.init_adam_optimizer(lr_update);
    
    // Prepare input in same shape as Python
    std::vector<std::vector<std::vector<float>>> train_input(1);
    train_input[0].push_back(past_errors);
    
    // Target should be single value in same shape as Python
    std::vector<float> target = {recent_error};
    
    float final_loss = 0.0f;
    for (int epoch = 0; epoch < epoch_update; ++epoch) {
        generator.zero_grad();
        
        auto predicted_val = generator.forward(train_input);
        float loss = compute_mse_loss(predicted_val, target);
        
        generator.backward(target, "MSE");
        generator.update_parameters_adam(lr_update);
        
        final_loss = loss;
    }
    
    generator.eval();  // Back to eval mode
    return final_loss;
}

void AnomalousThresholdGenerator::train() {
    is_training = true;
    generator.train();
}

void AnomalousThresholdGenerator::eval() {
    is_training = false;
    generator.eval();
}

float AnomalousThresholdGenerator::generate(const std::vector<float>& prediction_errors, float minimal_threshold) {
    generator.eval();
    
    std::vector<std::vector<std::vector<float>>> reshaped_input(1);
    reshaped_input[0].push_back(prediction_errors);
    
    auto threshold = generator.forward(reshaped_input);
    
    // Apply minimal threshold directly (like Python)
    float scaled_threshold = threshold[0];
    scaled_threshold = std::max(scaled_threshold, minimal_threshold);
    scaled_threshold = std::min(scaled_threshold, 0.2f);  // Cap maximum threshold
    
    return scaled_threshold;
}

std::pair<std::vector<std::vector<float>>, std::vector<std::vector<float>>> 
AnomalousThresholdGenerator::sliding_windows(const std::vector<float>& data, int window_size, int prediction_len) {
    std::vector<std::vector<float>> x, y;
    for (size_t i = window_size; i < data.size(); ++i) {
        x.push_back(std::vector<float>(data.begin() + i - window_size, data.begin() + i));
        y.push_back(std::vector<float>(data.begin() + i, std::min(data.begin() + i + prediction_len, data.end())));
    }
    return {x, y};
}

float AnomalousThresholdGenerator::compute_mse_loss(const std::vector<float>& output, const std::vector<float>& target) {
    float loss = 0.0f;
    for (size_t i = 0; i < output.size(); ++i) {
        float error = output[i] - target[i];
        loss += error * error;
    }
    return loss / output.size();
}

std::vector<float> AnomalousThresholdGenerator::compute_mse_loss_gradient(const std::vector<float>& output, const std::vector<float>& target) {
    std::vector<float> gradient(output.size());
    for (size_t i = 0; i < output.size(); ++i) {
        gradient[i] = 2.0f * (output[i] - target[i]) / output.size();
    }
    return gradient;
}
