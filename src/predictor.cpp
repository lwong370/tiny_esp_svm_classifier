#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "../liblinear/linear.h"

struct Model {
    struct model* model;
    int nr_class;
    int nr_feature;
    double bias;
    std::vector<int> labels;
};

// Function to load features from a file
std::vector<double> loadFeatures(const std::string& feature_file) {
    std::ifstream infile(feature_file);
    std::vector<double> features(13, 0.0);  // Initialize with 13 features

    if (!infile) {
        std::cerr << "Error opening feature file: " << feature_file << std::endl;
        exit(1);
    }

    int index;
    double value;
    while (infile >> index >> value) {
        if (index > 0 && static_cast<size_t>(index) <= features.size()) {
            features[index - 1] = value;
        }
    }

    return features;
}

// Original loadModel func before experimenting with new.model file
// Model loadModel(const std::string& model_file) {
//     Model model;
//     model.model = load_model(model_file.c_str());
//     // You might need to extract additional details from the model if required
//     return model;
// }

Model loadModel(const std::string& model_file) {
    Model model;
    model.model = load_model(model_file.c_str());
    if (model.model) {
        model.nr_class = model.model->nr_class;
        model.nr_feature = model.model->nr_feature;
        model.bias = model.model->bias;

        // Get labels
        model.labels.resize(model.nr_class);
        for (int i = 0; i < model.nr_class; ++i) {
            model.labels[i] = model.model->label[i];
        }
    } else {
        std::cerr << "Failed to load model from file: " << model_file << std::endl;
    }
    return model;
}


void prepare_features(double* raw_features, struct feature_node* x, int num_features) {
    for (int i = 0; i < num_features; ++i) {
        x[i].index = i + 1;  // LIBLINEAR uses 1-based indexing
        x[i].value = raw_features[i];
    }
    x[num_features].index = -1;  // End of feature node array
}

int main() {
    Model model = loadModel("heart_scale.model");

    // Prepare features
    double raw_features[13] = {0.5, 1.2, 0.3, 2.1, 1.5, 0.8, 1.0, 0.9, 2.3, 1.1, 0.7, 1.6, 1.4 }; // the feature vector of the new data we want to classify
    struct feature_node x[14];  // Number of features + 1 for end marker

    prepare_features(raw_features, x, 13);

    // Predict
    double prediction = predict(model.model, x);
    std::cout << "Predicted value: " << prediction << std::endl;

    // Clean up
    free_and_destroy_model(&model.model);

    return 0;
}
