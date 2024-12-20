#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "../liblinear/linear.h"
#define TAG "MY_APP"
#include "esp_log.h"
#include "esp_spiffs.h"


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

int run_prediction() {

    /////Loading Model as a file with SPIFF/////
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/storage",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t result = esp_vfs_spiffs_register(&config);
    if(result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        return -1;
    }

    // FILE* f = fopen("/storage/heart_scale.model", "r");
    // if(f == NULL) {
    //     ESP_LOGE("Failed to open file for reading");
    //     return -1;
    // }

    Model model = loadModel("/storage/svm_model.model");
    if (!model.model) {
        ESP_LOGE(TAG, "Failed to load model.");
        return -1;  // or handle the error appropriately
    }
    /////////////////////////////////////////////

    // BEFORE EXPERIMENT WITH NEW .MODEL FILE
    // Prepare features
    // double raw_features[13] = {0.5, 1.2, 0.3, 2.1, 1.5, 0.8, 1.0, 0.9, 2.3, 1.1, 0.7, 1.6, 1.4 }; // the feature vector of the new data we want to classify
    // struct feature_node x[14];  // Number of features + 1 for end marker

    // prepare_features(raw_features, x, 13);

    // // // Predict
    // double prediction = predict(model.model, x);
    // // std::cout << "Predicted value: " << prediction << std::endl;
    // ESP_LOGI(TAG, "My SVM classifier prediction: %f", prediction);

    // // // Clean up
    // free_and_destroy_model(&model.model);

    // return 0;
    //////////////////////////////////////////////////
    ESP_LOGI(TAG, "Model loaded successfully with %d classes and %d features.", model.nr_class, model.nr_feature);

    // Prepare input features (example values, replace with actual input)
   
    double raw_features[8] = {0.1, 0.2, 0.17, 0.4, 0.3, 0.6, 0.7, 0.42}; // Example input for 8 features

    struct feature_node* x = new feature_node[model.nr_feature + 1];
    prepare_features(raw_features, x, model.nr_feature);

    // Predict the class
    double prediction = predict(model.model, x);
    int predicted_label = model.labels[static_cast<int>(prediction) - 1]; // Map to label
    ESP_LOGI(TAG, "Predicted class: %d", predicted_label);

    // Clean up
    free_and_destroy_model(&model.model);
    delete[] x;

    return 0;
}


extern "C" void app_main(void)
{
    // static const char *TAG = "example";

    run_prediction();

    // Initializing GPIO22 as input with a pull up. 
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);

    // Initialize GPIO6 as output.
    gpio_set_direction(GPIO_NUM_6, GPIO_MODE_OUTPUT);

    while(true) {
        if(gpio_get_level(GPIO_NUM_2)) {
            gpio_set_level(GPIO_NUM_6, 0);
            ESP_LOGI(TAG, "Turning off the LED!");

        } else {
            gpio_set_level(GPIO_NUM_6, 1);
            ESP_LOGI(TAG, "Turning the LED!");
        }
        vTaskDelay(10);
    }
}
