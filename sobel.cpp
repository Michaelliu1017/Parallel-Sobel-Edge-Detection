// sobel.cpp
// Convert image: magick input.jpg -colorspace Gray input.pgm
// Compile: g++ -O2 -std=c++17 -fopenmp -o sobel sobel.cpp -lm
// Usage: ./sobel input.pgm output.pgm [l1|l2] [seq|par] [threads]

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <omp.h>
#include <unistd.h>

struct ImageU8 {
    int width = 0;
     int height = 0;
  std::vector<uint8_t> pixels;
};


static inline int getIndex(int x, int y, int width) {
    return y * width + x;
}

//**********************************************************************
// PGM File I/O
//**********************************************************************


static void skipWhitespaceAndComments(std::istream& in) {
    while (true) {
        int c = in.peek();
        if (c == '#') {
            // skip the entire comment line
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else if (std::isspace(c)) {
            in.get();
        } else {
            break;
        }
    }
}

// Read next integer from PGM header
static int readNextInt(std::istream& in) {
    skipWhitespaceAndComments(in);
    int value;
    in >> value;
    if (!in) {
        throw std::runtime_error("Error: could not read integer from PGM header.");
    }
    return value;
}

// Read a PGM file (supports both P2 ascii and P5 binary formats)
static ImageU8 readPGM(const std::string& filepath) {
    std::ifstream fin(filepath, std::ios::binary);
    if (!fin) {
        throw std::runtime_error("Error: cannot open file: " + filepath);
    }

    // Read magic number
    std::string magic;
    fin >> magic;
    if (!fin) {
        throw std::runtime_error("[Error]: failed to read PGM magic number.");
    }
    if (magic != "P5" && magic != "P2") {
        throw std::runtime_error("[Error]: unsupported PGM format '" + magic + "'.try again.");
    }

    // Read header fields
    int width   = readNextInt(fin);
     int height  = readNextInt(fin);
    int maxval  = readNextInt(fin);

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("[Error]: invalid image dimensions in PGM.");
    }
    if (maxval <= 0 || maxval > 255) {
        throw std::runtime_error("[Error]: PGM maxval must be between 1 and 255.");
    }

    

    fin.get();

    ImageU8 img;
    img.width  = width;
    img.height = height;
    img.pixels.resize(static_cast<size_t>(width) * height);

    if (magic == "P5") {
     
        fin.read(reinterpret_cast<char*>(img.pixels.data()), img.pixels.size());
        if (!fin) {
            throw std::runtime_error("[Error]: failed to read pixel data from P5 PGM.");
        }
    } else {
     
        for (size_t i = 0; i < img.pixels.size(); i++) {
            int val = readNextInt(fin);
            val = std::clamp(val, 0, maxval);
            img.pixels[i] = static_cast<uint8_t>(val * 255 / maxval);
        }
    }

    return img;
}

// Write image as binary PGM
static void writePGM(const std::string& filepath, const ImageU8& img) {
    std::ofstream fout(filepath, std::ios::binary);
    if (!fout) {
        throw std::runtime_error("[Error]: cannot open output file: " + filepath);
    }

    // Write PGM header
    fout << "P5\n" << img.width << " " << img.height << "\n255\n";

    // Write pixel data
    fout.write(reinterpret_cast<const char*>(img.pixels.data()), img.pixels.size());
    if (!fout) {
        throw std::runtime_error("[Error]: failed to write pixel data.");
    }
}

//**********************************************************************
// Sobel Kernel
//**********************************************************************

// Compute Sobel edge magnitude
static inline uint8_t computeSobelPixel(const ImageU8& input, int x, int y, bool use_l1) {
    const int w = input.width;

    // read 3x3 nearby
    const int p00 = input.pixels[getIndex(x-1, y-1, w)];
    const int p01 = input.pixels[getIndex(x,   y-1, w)];
    const int p02 = input.pixels[getIndex(x+1, y-1, w)];
    const int p10 = input.pixels[getIndex(x-1, y,   w)];

    const int p12 = input.pixels[getIndex(x+1, y,   w)];
    const int p20 = input.pixels[getIndex(x-1, y+1, w)];
    const int p21 = input.pixels[getIndex(x,   y+1, w)];
    const int p22 = input.pixels[getIndex(x+1, y+1, w)];

    // Apply tje kernel
    const int gx = (-p00 + p02) + (-2*p10 + 2*p12) + (-p20 + p22);
    const int gy = (-p00 - 2*p01 - p02) + (p20 + 2*p21 + p22);

    // Compute gradient magnitude
    int magnitude;
    if (use_l1) {
        magnitude = std::abs(gx) + std::abs(gy);
    } else {
        magnitude = static_cast<int>(std::lround(std::sqrt((double)gx*gx + (double)gy*gy)));
    }

    // Scale and clamp to [0, 255]
    const double scale = 255.0/1020.0;
     int value = static_cast<int>(magnitude * scale);
      value = std::clamp(value, 0, 255);

    return static_cast<uint8_t>(value);
}

//**********************************************************************
// Sequential Sobel
//**********************************************************************
ImageU8 sobelSequential(const ImageU8& input, bool use_l1) {
    ImageU8 output;
    output.width  = input.width;
    output.height = input.height;
     output.pixels.assign(static_cast<size_t>(input.width) * input.height, 0);

     const int w = input.width;
     const int h = input.height;

 

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            output.pixels[getIndex(x, y, w)] = computeSobelPixel(input, x, y, use_l1);
        }
    }

    return output;
}
//**********************************************************************
// Banner
//**********************************************************************
static void printBanner() {
    const std::string seed = "00080909000409490251409504612094067110920007719200086183000950930009923300095273000940940008454500080909";
    const std::string ORANGE = "\033[38;5;208m", DIM = "\033[38;5;202m", RESET = "\033[0m";
    const std::string BLK = "\xe2\x96\x88"; // UTF-8 █

    // Build lines
    std::vector<std::string> lines;
    for (int i = 0; i < 13; i++) {
        long long s = std::stoll(seed.substr(seed.size() - 8*(i+1), 8));
        std::string line;
        for (int j = 0; j < 8; j += 2) {
            int spaces = (s / (long long)std::pow(10, j))     % 10;
            int blocks = (s / (long long)std::pow(10, j + 1)) % 10;
            line += std::string(spaces, ' ');
            for (int b = 0; b < blocks; b++) line += BLK;
        }
        lines.push_back(line);
    }

    // Compute display
    int width = 0;
    for (auto& l : lines) {
        int w = 0;
        for (int k = 0; k < (int)l.size(); )
            if ((unsigned char)l[k] == 0xe2) { w++; k += 3; }
            else { w++; k++; }
        width = std::max(width, w);
    }

    int H = (int)lines.size();

    // Animation
    for (int reveal = 0; reveal <= H; reveal++) {
        for (int r = 0; r < H; r++) {
            int col = 0;
            for (int k = 0; k < (int)lines[r].size(); ) {
                if ((unsigned char)lines[r][k] == 0xe2) {
                    if (r < reveal)
                        std::cout << ORANGE << BLK << RESET;
                    else
                        std::cout << DIM << " " << RESET;
                    k += 3;
                } else {
                    std::cout << ' ';
                    k++;
                }
                col++;
            }
            while (col < width) { std::cout << ' '; col++; }
            std::cout << '\n';
        }
        usleep(80000);
        std::cout << "\033[" << H << "A" << std::flush;
    }

    // Final frame: all orange
    for (auto& l : lines) {
        int col = 0;
        for (int k = 0; k < (int)l.size(); ) {
            if ((unsigned char)l[k] == 0xe2) {
                std::cout << ORANGE << BLK << RESET;
                k += 3;
            } else {
                std::cout << ' ';
                k++;
            }
            col++;
        }
        while (col < width) { std::cout << ' '; col++; }
        std::cout << '\n';
    }
    std::cout << std::flush;

    // Info lines
    const std::string info[] = {
        "█ Parallel Sobel Edge Detection",
        "█ Environment : Linux x86_64 | GCC | OpenMP",
        "█ Version     : 2.1",
        "█ Usage       : ./sobel input.pgm output.pgm [l1|l2] [seq|par] [threads]"
    };
    std::cout << "\n";
    for (auto& line : info)
        std::cout << ORANGE << line << RESET << "\n";
    std::cout << "\n";
}
//**********************************************************************
// Parallel Sobel (OpenMP)
//**********************************************************************
ImageU8 sobelParallel(const ImageU8& input, bool use_l1, int numThreads) {
    ImageU8 output;
    output.width  = input.width;
    output.height = input.height;
     output.pixels.assign(static_cast<size_t>(input.width) * input.height, 0);

    const int w = input.width;
    const int h = input.height;

    omp_set_num_threads(numThreads);
    #pragma omp parallel for collapse(2) schedule(static) \
          default(none) shared(input, output, w, h, use_l1)
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
             output.pixels[getIndex(x, y, w)] = computeSobelPixel(input, x, y, use_l1);
        }
    }

     return output;
}

//**********************************************************************
// Correctness Check
//**********************************************************************

// Compare parallel output to sequential one pixels by pixels
void checkCorrectness(const ImageU8& seqOutput, const ImageU8& parOutput) {
    if (seqOutput.width != parOutput.width || seqOutput.height != parOutput.height) {
        std::cout << "[Check] FAILED: image dimensions do not match!\n";
        return;
    }

    int mismatchCount = 0;
    int maxDiff = 0;

    for (size_t i = 0; i < seqOutput.pixels.size(); i++) {
        int diff = std::abs((int)seqOutput.pixels[i] - (int)parOutput.pixels[i]);
        if (diff > 0) {
            mismatchCount++;
            if (diff > maxDiff) maxDiff = diff;
        }
    }

    if (mismatchCount == 0) {
        std::cout << "[Check] PASSED: parallel result is identical to sequential.\n";
    } else {
        std::cout << "[Check] FAILED: " << mismatchCount
                  << " pixels differ (max difference = " << maxDiff << ").\n";
    }
}

//**********************************************************************
//                              MAIN
//**********************************************************************
int main(int argc, char** argv) {
    if (argc == 1) printBanner();
    try {
        if (argc < 3) {
            std::cerr << "Usage:\n"
                      << "  " << argv[0] << " input.pgm output.pgm [mode] [seq|par] [threads]\n\n"
                      << "Arguments:\n"
                      << "  input.pgm  : grayscale input image (P2 or P5 PGM)\n"
                      << "  output.pgm : edge-detected output image\n"
                      << "  mode       : l1 (default) or l2\n"
                      << "               l1 = |Gx|+|Gy|  (faster)\n"
                      << "               l2 = sqrt(Gx^2+Gy^2)  (more accurate)\n"
                      << "  seq|par    : seq (default) or par\n"
                      << "  threads    : number of OpenMP threads for par mode (default: 4)\n\n"
                      << "Examples:\n"
                      << "  " << argv[0] << " input.pgm edges.pgm l1 seq\n"
                      << "  " << argv[0] << " input.pgm edges.pgm l1 par 8\n";
            return 1;
        }

        // Parse argument
        const std::string inputPath  = argv[1];
        const std::string outputPath = argv[2];
        std::string mode             = (argc >= 4) ? argv[3] : "l1";
        std::string execMode         = (argc >= 5) ? argv[4] : "seq";
        int numThreads               = (argc >= 6) ? std::stoi(argv[5]) : 4;

        // Validate mode
        bool use_l1 = true;
        if (mode == "l1") {
            use_l1 = true;
        } else if (mode == "l2") {
            use_l1 = false;
        } else {
            throw std::runtime_error("Unknown mode: '" + mode + "'. Use l1 or l2.");
          }

        // Validate execution mode
        if (execMode != "seq" && execMode != "par") {
            throw std::runtime_error("Unknown execution mode: '" + execMode + "'. Use seq or par.");
         }

        // Load input image
        std::cout << "Loading image: " << inputPath << "\n";
        ImageU8 inputImage = readPGM(inputPath);
        std::cout << "Image size: " << inputImage.width << "x" << inputImage.height << "\n";

        ImageU8 outputImage;

        if (execMode == "seq") {
            // --- Sequential version ---
            auto t0 = std::chrono::high_resolution_clock::now();
            outputImage = sobelSequential(inputImage, use_l1);
            auto t1 = std::chrono::high_resolution_clock::now();

            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "Sobel sequential (mode=" << mode << ") took " << ms << " ms\n";

        } else {
            // --- Parallel version ---
            auto t0 = std::chrono::high_resolution_clock::now();
            outputImage = sobelParallel(inputImage, use_l1, numThreads);
            auto t1 = std::chrono::high_resolution_clock::now();

            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "Sobel parallel (mode=" << mode << ", threads=" << numThreads
                      << ") took " << ms << " ms\n";

            // Run sequential version to comparee
            std::cout << "* Correctness Check...\n";
            ImageU8 seqReference = sobelSequential(inputImage, use_l1);
            checkCorrectness(seqReference, outputImage);
        }

        // Save it
        writePGM(outputPath, outputImage);
        std::cout << "* Output written to: " << outputPath << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
}
