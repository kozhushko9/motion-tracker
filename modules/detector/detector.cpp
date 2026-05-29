#include "detector.hpp"

#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>

#include <cstring>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

#include "config.hpp"
#include "scoped_timer.hpp"
#include "vision_utils.hpp"

Detector::Detector(const AppConfig& cfg) : config_(cfg) {
    // Pre-allocate vectors once here
    boxes_.reserve(128);
    scores_.reserve(128);
    class_ids_.reserve(128);

    std::cout << "\n[TFLite] Loading model: " << config_.model.path << "\n";

    // Load the model
    model_ = tflite::FlatBufferModel::BuildFromFile(config_.model.path.c_str());
    if (!model_) {
        throw std::runtime_error("\n[TFLite] Failed to load TFLite model\n");
    }

    // Build the interpreter
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder(*model_, resolver)(&interpreter_);
    if (!interpreter_) {
        throw std::runtime_error("\n[TFLite] Failed to create interpreter\n");
    }

    TfLiteXNNPackDelegateOptions xnnpack_options = TfLiteXNNPackDelegateOptionsDefault();

    xnnpack_options.num_threads = config_.detector.num_threads;

    xnnpack_delegate_.reset(
        TfLiteXNNPackDelegateCreate(&xnnpack_options));

    if (!xnnpack_delegate_) {
        throw std::runtime_error("[TFLite] Failed to create XNNPACK delegate");
    }

    if (interpreter_->ModifyGraphWithDelegate(xnnpack_delegate_.get()) != kTfLiteOk) {
        throw std::runtime_error("[TFLite] Failed to apply XNNPACK delegate");
    }

    // Allocate tensors
    if (interpreter_->AllocateTensors() != kTfLiteOk) {
        throw std::runtime_error("Failed to allocate TFLite tensors");
    }

    input_tensor_ = interpreter_->tensor(interpreter_->inputs()[0]);
    output_tensor_ = interpreter_->tensor(interpreter_->outputs()[0]);

    std::cout << "\n[TFLite] Model loaded successfully\n\n";
}

void Detector::detect(const Frame& frame, DetectionResult& result) {
    ScopedTimer total_timer("detector.total");

    TfLiteIntArray* input_dims = input_tensor_->dims;  // shape

    int input_h = input_dims->data[1];
    int input_w = input_dims->data[2];

    if (input_h != input_w) {
        throw std::runtime_error("[TFLite] Only square YOLO input is currently supported");
    }

    int target_size = input_h;

    // Need these later for mapping boxes back to original image
    int top = 0;
    int left = 0;        // padding
    float scale = 1.0f;  // letterbox scale

    cv::Mat input_img;

    {
        ScopedTimer preprocess_timer("detector.preprocess");

        vision::letterbox(
            frame.image,
            scratch_letterbox_,
            scratch_resized_,
            target_size,
            top,
            left,
            scale);
        cv::cvtColor(scratch_letterbox_, scratch_rgb_, cv::COLOR_BGR2RGB);

        input_img = scratch_rgb_;

        // ===== Fill input tensor =====
        if (input_tensor_->type == kTfLiteFloat32) {
            float* input_ptr = interpreter_->typed_input_tensor<float>(0);
            cv::Mat target_mat(input_h, input_w, CV_32FC3, input_ptr);
            input_img.convertTo(target_mat, CV_32F, 1.0 / 255.0);

        } else if (input_tensor_->type == kTfLiteInt8) {
            int8_t* input_ptr = interpreter_->typed_input_tensor<int8_t>(0);

            const float input_scale = input_tensor_->params.scale;
            const int input_zero_point = input_tensor_->params.zero_point;

            if (input_scale <= 0.0f) {
                throw std::runtime_error("[TFLite] Invalid int8 input quantization scale");
            }

            // dst = src * alpha + beta
            // (val / 255.0) / scale + zero_point
            const double alpha = 1.0 / (255.0 * input_scale);
            const double beta = static_cast<double>(input_zero_point);

            cv::Mat target_mat(input_h, input_w, CV_8SC3, input_ptr);
            input_img.convertTo(target_mat, CV_8S, alpha, beta);

        } else if (input_tensor_->type == kTfLiteUInt8) {
            uint8_t* input_ptr = interpreter_->typed_input_tensor<uint8_t>(0);

            const float input_scale = input_tensor_->params.scale;
            const int input_zero_point = input_tensor_->params.zero_point;

            if (input_scale <= 0.0f) {
                throw std::runtime_error("[TFLite] Invalid uint8 input quantization scale");
            }

            const double alpha = 1.0 / (255.0 * input_scale);
            const double beta = static_cast<double>(input_zero_point);

            cv::Mat target_mat(input_h, input_w, CV_8UC3, input_ptr);
            input_img.convertTo(target_mat, CV_8U, alpha, beta);

        } else {
            throw std::runtime_error("[TFLite] Unsupported input tensor type");
        }
    }

    {
        ScopedTimer invoke_timer("detector.invoke");

        // Inference
        if (interpreter_->Invoke() != kTfLiteOk) {
            throw std::runtime_error("\n[TFLite] Inference failed\n");
        }
    }

    boxes_.clear();
    scores_.clear();
    class_ids_.clear();

    const float inverted_scale = 1.0f / scale;

    auto process_prediction = [&](float x1, float y1, float x2, float y2, float score, int cls) {
        if (score > config_.detector.conf_threshold && config_.detector.class_names.count(cls)) {
            x1 *= static_cast<float>(target_size);
            y1 *= static_cast<float>(target_size);
            x2 *= static_cast<float>(target_size);
            y2 *= static_cast<float>(target_size);

            // model input pixels -> original frame pixels
            x1 = (x1 - static_cast<float>(left)) * inverted_scale;
            y1 = (y1 - static_cast<float>(top)) * inverted_scale;
            x2 = (x2 - static_cast<float>(left)) * inverted_scale;
            y2 = (y2 - static_cast<float>(top)) * inverted_scale;

            // clip
            x1 = std::clamp(x1, 0.0f, static_cast<float>(frame.image.cols - 1));
            y1 = std::clamp(y1, 0.0f, static_cast<float>(frame.image.rows - 1));
            x2 = std::clamp(x2, 0.0f, static_cast<float>(frame.image.cols - 1));
            y2 = std::clamp(y2, 0.0f, static_cast<float>(frame.image.rows - 1));

            int x = static_cast<int>(std::round(x1));
            int y = static_cast<int>(std::round(y1));
            int w = static_cast<int>(std::round(x2 - x1));
            int h = static_cast<int>(std::round(y2 - y1));

            if (w > 1 && h > 1) {
                boxes_.emplace_back(x, y, w, h);
                scores_.push_back(score);
                class_ids_.push_back(cls);
            }
        }
    };

    // Validating output tensor shape before decoding
    if (output_tensor_->dims->size != 3) {
        throw std::runtime_error("[TFLite] Expected output shape [1, N, 6]");
    }

    if (output_tensor_->dims->data[0] != 1) {
        throw std::runtime_error("[TFLite] Expected batch size 1 in output tensor");
    }

    if (output_tensor_->dims->data[2] != 6) {
        throw std::runtime_error("[TFLite] Expected YOLO output with 6 elements per prediction");
    }

    {
        ScopedTimer decode_timer("detector.decode");

        const int num_preds = output_tensor_->dims->data[1];
        const int elements = output_tensor_->dims->data[2];

        if (output_tensor_->type == kTfLiteFloat32) {
            float* output_ptr = interpreter_->typed_output_tensor<float>(0);

            for (int i = 0; i < num_preds; i++) {
                float* det = output_ptr + (i * elements);
                process_prediction(
                    det[0],
                    det[1],
                    det[2],
                    det[3],
                    det[4],
                    static_cast<int>(std::round(det[5])));
            }
        } else if (output_tensor_->type == kTfLiteInt8) {
            int8_t* output_ptr = interpreter_->typed_output_tensor<int8_t>(0);

            const float output_scale = output_tensor_->params.scale;
            const int output_zero_point = output_tensor_->params.zero_point;

            auto dequant = [&](int8_t q) {
                return (static_cast<float>(q) - output_zero_point) * output_scale;
            };

            for (int i = 0; i < num_preds; i++) {
                int8_t* det = output_ptr + (i * elements);
                process_prediction(
                    dequant(det[0]),
                    dequant(det[1]),
                    dequant(det[2]),
                    dequant(det[3]),
                    dequant(det[4]),
                    static_cast<int>(std::round(dequant(det[5]))));
            }
        } else {
            throw std::runtime_error("[TFLite] Unsupported output tensor type");
        }
    }

    result.boxes.assign(boxes_.begin(), boxes_.end());
    result.scores.assign(scores_.begin(), scores_.end());
    result.class_ids.assign(class_ids_.begin(), class_ids_.end());
}