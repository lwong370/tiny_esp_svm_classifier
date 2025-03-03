#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include "../liblinear/linear.h"
#define TAG "MY_APP"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/adc.h"  
#include "esp_adc/adc_oneshot.h"

#define FSR_PIN ADC_CHANNEL_7 // Change this based on your ESP32-S3 board
#define SAMPLES 100

// static const char *TAG = "FSR_SENSOR";
struct Model {
    struct model* model;
    int nr_class;
    int nr_feature;
    double bias;
    std::vector<int> labels;
};

std::map<int, std::string> label_map = {
    {1, "CR"},
    {2, "CW"},
    {3, "IF"},
    {4, "IP"},
    {5, "KP"},
    {6, "PP"},
    {7, "TP"},
    {8, "WE"},
    {9, "WF"},
    {10, "WR"}
};

// FSR CODE
void read_fsr_task(void *pvParameter) {
    // Initialize ADC handle and configuration for ADC one-shot mode
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,  // Default clock source
        .ulp_mode = ADC_ULP_MODE_DISABLE    // No ULP mode
    };

    // Initialize ADC one-shot unit
    adc_oneshot_new_unit(&init_config, &adc1_handle);

    // Configure the ADC channel for the FSR sensor pin
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,  // 12dB attenuation (0-3.3V range)
        .bitwidth = ADC_BITWIDTH_DEFAULT   // 12-bit resolution
    };
    esp_err_t err = adc_oneshot_config_channel(adc1_handle, FSR_PIN, &channel_config);
    if (err != ESP_OK) {
        printf("Failed to configure ADC channel: %d\n", err);
        adc_oneshot_del_unit(adc1_handle);  // Cleanup before exiting
        vTaskDelete(NULL);
    }

    while (1) {
        // Read the raw ADC value
        int raw_value;
        esp_err_t err = adc_oneshot_read(adc1_handle, FSR_PIN, &raw_value);
        if (err == ESP_OK) {
            printf("FSR Reading: %d\n", raw_value);
        } else {
            printf("ADC Read Failed: %d\n", err);
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // Delay for 100ms (adjust as needed)
    }

    adc_oneshot_del_unit(adc1_handle);
}

// Function to load features from a file
std::vector<double> loadFeatures(const std::string& feature_file) {
    std::ifstream infile(feature_file);
    std::vector<double> features(8, 0.0); 

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

Model loadModel(const std::string& model_file) {
    Model model;
    model.model = load_model(model_file.c_str());
    
    if (model.model) {
        // Populate model metadata
        model.nr_class = model.model->nr_class;
        model.nr_feature = model.model->nr_feature;
        model.bias = model.model->bias;

        // Populate class labels
        model.labels.resize(model.nr_class);
        for (int i = 0; i < model.nr_class; ++i) {
            model.labels[i] = model.model->label[i];
        }

        // Debugging: Log loaded model properties
        std::cout << "Model loaded successfully:\n"
                  << "Classes: " << model.nr_class << "\n"
                  << "Features: " << model.nr_feature << "\n"
                  << "Bias: " << model.bias << "\n"
                  << "Labels: ";
        for (const auto& label : model.labels) {
            std::cout << label << " ";
        }
        std::cout << std::endl;
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
    // Initialize SPIFFS to read model file
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/storage",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    // Check if SPIFFs initialization faled
    esp_err_t result = esp_vfs_spiffs_register(&config);
    if(result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        return -1;  // Exit on failure
    }

    // Loading Model as a file with SPIFF
    Model model = loadModel("/storage/svm_model.model");
    if (!model.model) {
        ESP_LOGE(TAG, "Failed to load model.");
        return -1;  // or handle the error appropriately
    }

    ESP_LOGI(TAG, "Model loaded successfully with %d classes and %d features.", model.nr_class, model.nr_feature);

    // Prepare input features (example values, replace with actual input)
    double raw_features[8] = {0.0010971067573347477, 4.376688746554962, 2.5435292875537745, 0.007368199818891945, 0.23860235784040276, 0.02101373043442002, 0.8792933147990581, 0.013699633994590819}; // Example input for 8 features

    struct feature_node* x = new feature_node[model.nr_feature + 1]; // Allocate feature nodes
    prepare_features(raw_features, x, model.nr_feature); // Format data for prediction with LibLinear

    // Predict the class
    double prediction = predict(model.model, x);
    int predicted_label = model.labels[static_cast<int>(prediction) - 1]; // Map to label
    ESP_LOGI(TAG, "Predicted class: %s", label_map[predicted_label].c_str());

    // Clean up
    free_and_destroy_model(&model.model);
    delete[] x;

    // Return Success    
    return 0;
}


extern "C" void app_main(void) {
    // FSR CODE
    xTaskCreate(&read_fsr_task, "read_fsr_task", 4096, NULL, 5, NULL);
    
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
