#include <cctype>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "Image.h"

namespace fs = std::filesystem;

namespace {

struct Task {
    fs::path inputPath;
    fs::path outputPath;
};

enum class Operation { Grayscale = 1, Invert = 2, GrayscaleInvert = 3 };
enum class Mode { Sequential = 1, Parallel = 2 };

bool isSupportedImage(const fs::path& path) {
    if (!path.has_extension()) {
        return false;
    }
    std::string ext = path.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".tga";
}

std::vector<fs::path> findImages(const fs::path& inputDir) {
    std::vector<fs::path> images;
    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (isSupportedImage(entry.path())) {
            images.push_back(entry.path());
        }
    }
    return images;
}

fs::path makeOutputPath(const fs::path& outputDir, const fs::path& inputPath) {
    // photo.jpg -> photo.jpg.processed.png
    return outputDir / (inputPath.filename().string() + ".processed.png");
}

void applyOperation(Image& image, Operation op) {
    switch (op) {
        case Operation::Grayscale:
            image.grayscale();
            break;
        case Operation::Invert:
            image.invert();
            break;
        case Operation::GrayscaleInvert:
            image.grayscale();
            image.invert();
            break;
    }
}

bool processOne(const Task& task, Operation op) {
    Image image;
    if (!image.load(task.inputPath.string())) {
        return false;
    }
    applyOperation(image, op);
    return image.save(task.outputPath.string());
}

const char* operationName(Operation op) {
    switch (op) {
        case Operation::Grayscale:
            return "Grayscale";
        case Operation::Invert:
            return "Invert";
        case Operation::GrayscaleInvert:
            return "Grayscale + Invert";
    }
    return "Unknown";
}

void printSummary(Mode mode,
                  Operation op,
                  int workers,
                  std::size_t total,
                  std::size_t success,
                  std::size_t failed,
                  double seconds,
                  const fs::path& outputDir) {
    std::cout << "\n========================================\n"
              << "          PROCESSING SUMMARY\n"
              << "========================================\n"
              << "Mode:                    "
              << (mode == Mode::Parallel ? "Parallel" : "Sequential") << '\n'
              << "Operation:               " << operationName(op) << '\n';
    if (mode == Mode::Parallel) {
        std::cout << "Worker threads:          " << workers << '\n';
    }
    std::cout << "Total images:            " << total << '\n'
              << "Successfully processed:  " << success << '\n'
              << "Failed:                  " << failed << '\n'
              << "Processing time:         " << seconds << " seconds\n"
              << "Results directory:       " << outputDir.string() << '\n'
              << "========================================\n";
}

void processSequential(const std::vector<Task>& tasks,
                       Operation op,
                       std::size_t& success,
                       std::size_t& failed) {
    for (const Task& task : tasks) {
        if (processOne(task, op)) {
            ++success;
            std::cout << "Completed: " << task.inputPath.filename().string() << '\n';
        } else {
            ++failed;
            std::cout << "Failed: " << task.inputPath.filename().string() << '\n';
        }
    }
}

void processParallel(std::queue<Task>& queue,
                     Operation op,
                     int workers,
                     std::size_t& success,
                     std::size_t& failed) {
    std::mutex queueMutex;
    std::mutex statisticsMutex;
    std::mutex outputMutex;

    auto worker = [&](int threadId) {
        for (;;) {
            Task task;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (queue.empty()) {
                    return;
                }
                task = queue.front();
                queue.pop();
            }

            const bool ok = processOne(task, op);

            {
                std::lock_guard<std::mutex> lock(outputMutex);
                if (ok) {
                    std::cout << "[Thread " << threadId << "] Completed: "
                              << task.inputPath.filename().string() << '\n';
                } else {
                    std::cout << "[Thread " << threadId << "] Failed: "
                              << task.inputPath.filename().string() << '\n';
                }
            }

            {
                std::lock_guard<std::mutex> lock(statisticsMutex);
                if (ok) {
                    ++success;
                } else {
                    ++failed;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(workers));
    for (int i = 1; i <= workers; ++i) {
        threads.emplace_back(worker, i);
    }
    for (std::thread& t : threads) {
        t.join();
    }
}

}  // namespace

int main() {
    std::cout << "========================================\n"
              << "       IMAGE PROCESSING UTILITY\n"
              << "========================================\n";

    std::string inputDirStr;
    std::string outputDirStr;
    std::cout << "Enter input directory:  ";
    if (!std::getline(std::cin, inputDirStr)) {
        std::cerr << "Error: failed to read input directory.\n";
        return 1;
    }

    std::cout << "Enter output directory: ";
    if (!std::getline(std::cin, outputDirStr)) {
        std::cerr << "Error: failed to read output directory.\n";
        return 1;
    }

    const fs::path inputDir(inputDirStr);
    const fs::path outputDir(outputDirStr);

    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "Error: input directory does not exist.\n";
        return 1;
    }

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "Error: cannot create output directory.\n";
        return 1;
    }

    std::cout << "\nScanning input directory...\n";
    const std::vector<fs::path> images = findImages(inputDir);
    std::cout << "Found " << images.size() << " supported image files.\n";
    if (images.empty()) {
        std::cerr << "No supported images found. Exiting.\n";
        return 1;
    }

    std::cout << "\nSelect processing operation:\n"
              << "  1. Grayscale\n"
              << "  2. Invert\n"
              << "  3. Grayscale + Invert\n"
              << "Enter operation number: ";
    int opNum = 0;
    if (!(std::cin >> opNum) || opNum < 1 || opNum > 3) {
        std::cerr << "Error: invalid operation number.\n";
        return 1;
    }
    const Operation op = static_cast<Operation>(opNum);

    std::cout << "\nSelect processing mode:\n"
              << "  1. Sequential processing\n"
              << "  2. Parallel processing\n"
              << "Enter mode: ";
    int modeNum = 0;
    if (!(std::cin >> modeNum) || modeNum < 1 || modeNum > 2) {
        std::cerr << "Error: invalid mode.\n";
        return 1;
    }
    const Mode mode = static_cast<Mode>(modeNum);

    int workers = 1;
    if (mode == Mode::Parallel) {
        std::cout << "\nEnter number of worker threads (> 0): ";
        if (!(std::cin >> workers) || workers <= 0) {
            std::cerr << "Error: invalid number of threads.\n";
            return 1;
        }
    }

    std::vector<Task> tasks;
    tasks.reserve(images.size());
    for (const fs::path& img : images) {
        tasks.push_back(Task{img, makeOutputPath(outputDir, img)});
    }

    std::size_t success = 0;
    std::size_t failed = 0;

    const auto t0 = std::chrono::steady_clock::now();
    if (mode == Mode::Sequential) {
        processSequential(tasks, op, success, failed);
    } else {
        std::queue<Task> queue;
        for (const Task& task : tasks) {
            queue.push(task);
        }
        processParallel(queue, op, workers, success, failed);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double seconds =
        std::chrono::duration<double>(t1 - t0).count();

    printSummary(mode, op, workers, tasks.size(), success, failed, seconds,
                 outputDir);
    return 0;
}
