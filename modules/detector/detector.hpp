#pragma once
#include <memory>
#include <vector>

#include "config.hpp"
#include "detection_result.hpp"
#include "frame.hpp"

// TFLite
#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>

struct XNNPackDelegateDeleter {
    void operator()(TfLiteDelegate* delegate) const {
        if (delegate) {
            TfLiteXNNPackDelegateDelete(delegate);
        }
    }
};

class Detector {
   public:
    explicit Detector(const AppConfig& cfg);

    void detect(const Frame& frame, DetectionResult& result);

   private:
    AppConfig config_;

    // TFLite
    std::unique_ptr<tflite::FlatBufferModel> model_;
    std::unique_ptr<tflite::Interpreter> interpreter_;

    TfLiteTensor* input_tensor_ = nullptr;
    TfLiteTensor* output_tensor_ = nullptr;

    std::unique_ptr<TfLiteDelegate, XNNPackDelegateDeleter> xnnpack_delegate_;

    std::vector<cv::Rect> boxes_;
    std::vector<float> scores_;
    std::vector<int> class_ids_;

    cv::Mat scratch_letterbox_;
    cv::Mat scratch_resized_;
    cv::Mat scratch_rgb_;
};