#include <cstdlib>
#include <exception>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

/* --- Global Constants --- */

constexpr char asciiChars[] = {'@', '%', '#', '*', '+', '=', '-', ':', '.', ' '};
constexpr int asciiLen = sizeof(asciiChars);

constexpr int DEFAULT_TARGET_HEIGHT = 60;
constexpr int DEFAULT_TARGET_WIDTH  = 0;     // Auto detect from aspect ratio
constexpr int MIN_HEIGHT            = 20;
constexpr int MIN_WIDTH             = 40;
constexpr int MAX_HEIGHT            = 120;
constexpr int MAX_WIDTH             = 200;

constexpr int DEFAULT_FRAMERATE = 30;
constexpr int MIN_FRAMERATE     = 1;
constexpr int MAX_FRAMERATE     = 120;
constexpr int MAX_FRAME_COUNT   = 100000;

enum class ColorMode : uint8_t {
    None,
    ANSI,
    Full
};

namespace Color {
    constexpr const char* BLACK = "\x1b[30m";
    constexpr const char* RED   = "\x1b[31m";
    constexpr const char* GREEN = "\x1b[32m";
    constexpr const char* BLUE  = "\x1b[34m";
    constexpr const char* WHITE = "\x1b[37m";

    constexpr const char* BRIGHT_BLACK = "\x1b[90m";
    constexpr const char* BRIGHT_RED   = "\x1b[91m";
    constexpr const char* BRIGHT_GREEN = "\x1b[92m";
    constexpr const char* BRIGHT_BLUE  = "\x1b[94m";
    constexpr const char* BRIGHT_WHITE = "\x1b[97m";

    constexpr const char* TRUECOLOR = "\x1b[38;2;";
    constexpr const char* RESET = "\x1b[0m";

    constexpr int DARK_THRESHOLD = 30;
    constexpr int GRAYSCALE_VARIANCE = 20;
    constexpr int VERY_BRIGHT = 200;
    constexpr int BRIGHT = 120;
    constexpr int MEDIUM_BRIGHT = 128;
}

/* --- Custom Types --- */

struct Options {
    const char* videoPath;
    ColorMode colorMode     = ColorMode::None;
    int targetHeight        = DEFAULT_TARGET_HEIGHT;
    int targetWidth         = DEFAULT_TARGET_WIDTH;
    int framerate           = -1;
};

/* --- Function Prototypes --- */

int getOptions(Options &opts, int argc, char** argv);
void getTargetDimensions(const cv::VideoCapture& cap, Options& opts);
double getDelayMs(const cv::VideoCapture& cap, const Options& opts);
void loadFrames(cv::VideoCapture& cap, std::vector<std::string>& asciiFrames,
        const Options& opts, int height, int width);
void animateAscii(const std::vector<std::string>& asciiFrames, double delayMs);
inline char brightnessToAscii(int brightness);
inline const char* rgbToAnsiColor(int r, int g, int b, int brightness);
inline std::string rgbToTrueColor(int r, int g, int b, int brightness);
void printHelp();
void clearScreen();

/* --- Main --- */

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ASCIIAnimator <video_path> [options]\n";
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0) {
        printHelp();
        return 1;
    } 

    Options opts;
    if (getOptions(opts, argc, argv) == 1) {
        return 1;
    }

    cv::VideoCapture cap(opts.videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video\n";
        return 1;
    }

    getTargetDimensions(cap, opts);

    std::vector<std::string> asciiFrames;
    auto frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    if (frameCount > 0 && frameCount < MAX_FRAME_COUNT) {
        asciiFrames.reserve(static_cast<size_t>(frameCount));
    }

    double delayMs = getDelayMs(cap, opts);

    loadFrames(cap, asciiFrames, opts, opts.targetHeight, opts.targetWidth);
    animateAscii(asciiFrames, delayMs);

    return 0;
}

/* --- Function Definitions --- */

int getOptions(Options &opts, int argc, char** argv) {
    opts.videoPath = argv[1];

    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "--color=", 8) == 0) {
            std::string mode = argv[i] + 8; // Truncate "--color="
            if      (mode == "none") { opts.colorMode = ColorMode::None; }
            else if (mode == "ansi") { opts.colorMode = ColorMode::ANSI; }
            else if (mode == "full") { opts.colorMode = ColorMode::Full; }
            else {
                std::cerr << "Unknown color mode: " << mode << '\n';
                return 1;
            }
        } else if (strncmp(argv[i], "--height=", 9) == 0) {
            try {
                int targetHeight = std::stoi(argv[i] + 9);
                if (targetHeight < MIN_HEIGHT || targetHeight > MAX_HEIGHT) {
                    std::cerr << "Error: Target height is out of bounds\n";
                    return 1;
                }
                opts.targetHeight = targetHeight;
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid height value\n";
                return 1;
            }
        } else if (strncmp(argv[i], "--width=", 8) == 0) {
            try {
                int targetWidth = std::stoi(argv[i] + 8);
                if (targetWidth < MIN_WIDTH || targetWidth > MAX_WIDTH) {
                    std::cerr << "Error: Target width is out of bounds\n";
                    return 1;
                }
                opts.targetWidth = targetWidth;
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid width value\n";
                return 1;
            }
        } else if (strncmp(argv[i], "--framerate=", 12) == 0) {
            try {
                int framerate = std::stoi(argv[i] + 12);
                if (framerate < MIN_FRAMERATE || framerate > MAX_FRAMERATE) {
                    std::cerr << "Error: Target framerate is out of bounds\n";
                    return 1;
                }
                opts.framerate = framerate;
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid framerate value\n";
                return 1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            printHelp();
            return 1;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            std::cerr << "Use --help for usage information\n";
            return 1;
        }
    }

    return 0;
}

void getTargetDimensions(const cv::VideoCapture& cap, Options& opts) {
    double videoWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double videoHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double videoAspect = videoWidth / videoHeight;
    double charAspect = 0.5;

    if (opts.targetWidth <= 0) {
        opts.targetWidth = static_cast<int>(opts.targetHeight * videoAspect / charAspect);
    }

    opts.targetHeight = std::clamp(opts.targetHeight, MIN_HEIGHT, MAX_HEIGHT);
    opts.targetWidth = std::clamp(opts.targetWidth, MIN_WIDTH, MAX_WIDTH);
}

double getDelayMs(const cv::VideoCapture& cap, const Options& opts) {
    if (opts.framerate != -1) { return 1000.0 / opts.framerate; }

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) { fps = DEFAULT_FRAMERATE; }
    return 1000.0 / fps;
}

void loadFrames(cv::VideoCapture& cap, std::vector<std::string>& asciiFrames,
        const Options& opts, int height, int width) {
    cv::Mat frame, gray, resized, resizedColor;
    const cv::Size size(width, height);

    while (cap.read(frame)) {
        std::ostringstream frameStream;

        if (opts.colorMode == ColorMode::ANSI || opts.colorMode == ColorMode::Full) {
            cv::resize(frame, resizedColor, size, 0, 0, cv::INTER_AREA);
        } else {
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
            cv::resize(gray, resized, size, 0, 0, cv::INTER_AREA);
        }

        for (int y = 0; y < height; y++) {
            const cv::Vec3b* colorRowPtr = (opts.colorMode != ColorMode::None)
                ? resizedColor.ptr<cv::Vec3b>(y) : nullptr;
            const uchar* grayRowPtr = (opts.colorMode == ColorMode::None)
                ? resized.ptr<uchar>(y) : nullptr;

            for (int x = 0; x < width; x++) {
                if (opts.colorMode == ColorMode::ANSI) {
                    const cv::Vec3b& px = colorRowPtr[x];
                    uchar b = px[0], g = px[1], r = px[2];

                    int brightness = std::clamp((r + g + b) / 3, 0, 255);

                    frameStream << rgbToAnsiColor(r, g, b, brightness);
                    frameStream << brightnessToAscii(brightness);
                    frameStream << Color::RESET;
                } else if (opts.colorMode == ColorMode::Full) {
                    const cv::Vec3b& px = colorRowPtr[x];
                    uchar b = px[0], g = px[1], r = px[2];

                    int brightness = std::clamp((r + g + b) / 3, 0, 255);

                    frameStream << rgbToTrueColor(r, g, b, brightness);
                } else {
                    uchar px = grayRowPtr[x];
                    int brightness = std::clamp(static_cast<int>(px), 0, 255);
                    int index = brightness * (asciiLen - 1) / 255;
                    frameStream << asciiChars[index];
                }
            }
            frameStream << '\n';
        }
        asciiFrames.push_back(frameStream.str());
    }
}

void animateAscii(const std::vector<std::string>& asciiFrames, double delayMs) {
    for (const auto& frameStr : asciiFrames) {
        clearScreen();
        std::cout << frameStr << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delayMs)));
    }
}

inline char brightnessToAscii(int brightness) {
    int index = brightness * (asciiLen - 1) / 255;
    return asciiChars[index];
}

inline const char* rgbToAnsiColor(int r, int g, int b, int brightness) {
    if (brightness < Color::DARK_THRESHOLD) { return Color::BLACK; }

    // Check for grayscale (low color variance)
    int maxC = std::max({r, g, b});
    int minC = std::min({r, g, b});
    if (maxC - minC < Color::GRAYSCALE_VARIANCE) {
        if (brightness > Color::VERY_BRIGHT) { return Color::BRIGHT_WHITE; }
        if (brightness > Color::BRIGHT) { return Color::WHITE; }
        return Color::BRIGHT_BLACK;
    }

    // Classify color
    bool bright = brightness > Color::MEDIUM_BRIGHT;
    if (r > g && r > b) { return bright ? Color::BRIGHT_RED : Color::RED; }
    if (g > r && g > b) { return bright ? Color::BRIGHT_GREEN : Color::GREEN; }
    if (b > r && b > g) { return bright ? Color::BRIGHT_BLUE : Color::BLUE; }

    return Color::WHITE;
}

inline std::string rgbToTrueColor(int r, int g, int b, int brightness) {
    std::string result;
    result.reserve(32);
    result += Color::TRUECOLOR;
    result += std::to_string(r);
    result += ';';
    result += std::to_string(g);
    result += ';';
    result += std::to_string(b);
    result += 'm';
    result += brightnessToAscii(brightness);
    result += Color::RESET;

    return result;
}

void printHelp() {
    std::cerr << "Usage: ASCIIAnimator <video_path> [options]\n\n"
              << "Options:\n"

              << "  --color=<mode>  Color mode: none, ansi, full (default: none)\n"

              << "  --height=<n>    Target height in num chars "
              << "[" << MIN_HEIGHT << ", " << MAX_HEIGHT << "] "
              << "(default: " << DEFAULT_TARGET_HEIGHT << ")\n"

              << "  --width=<n>     Target width in num chars  "
              << "[" << MIN_WIDTH << ", " << MAX_WIDTH << "] "
              << "(default: auto)\n"

              << "  --framerate=<n> Target frames per second   "
              << "[" << MIN_FRAMERATE << ", " << MAX_FRAMERATE << "] "
              << "(default: auto)\n"

              << "  --help          Show this help message\n";
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    std::cout << "\033[2J\033[H" << std::flush;
#endif
}
