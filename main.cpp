#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>
#include <cstdlib>
#include <iterator>
#include <thread>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

using namespace std;

constexpr char ramp[] = " .:-=+*#%@";
constexpr int EDGE_THRESHOLD = 90;
constexpr float pi = 3.14159265358979323846f;

// Kernels for Sobel filter
constexpr int Gx[3][3] = {
    {-1, 0, 1}, 
    {-2, 0, 2}, 
    {-1, 0, 1}
};
constexpr int Gy[3][3] = {
    {-1, -2, -1}, 
    {0, 0, 0}, 
    {1, 2, 1}
};

struct AsciiCell {
    float luminance;
    char symbol;
    int r;
    int g;
    int b;
};

struct MediaInfo {
    std::string filename;
    std::vector<AsciiCell> cells; 
    int width;
    int height; 
    int color_channels; 
    int output_width;
    double char_aspect_ratio;
};

/**
 * @brief Convert sRGB value to linear RGB value.
 * 
 * @param sRGB sRGB value (0.0 - 1.0)
 * @return float Linear RGB value (0.0 - 1.0)
 */
float sRGB_to_linear(float sRGB) {
    if (sRGB <= 0.04045) {
        return sRGB / 12.92;
    } else {
        return pow((sRGB + 0.055) / 1.055, 2.4);
    }
}

/**
 * @brief Calculate the perceived luminance of a color given its RGB values.
 * 
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return float Perceived luminance value (0-100)
 */
float perceived_luminance(int r, int g, int b) {
    float vR = sRGB_to_linear(r / 255.0);
    float vG = sRGB_to_linear(g / 255.0);
    float vB = sRGB_to_linear(b / 255.0);
    
    float luminance = vR * 0.2126 + vG * 0.7152 + vB * 0.0722;

    float pl;
    
    if (luminance <= 0.008856) {
        pl = luminance * 903.3;
    } else {
        pl = pow(luminance, 1.0 / 3.0) * 116 - 16;
    }

    return pl;
}

void normalize_values_and_assign_char(std::vector<AsciiCell> &cells, 
                                      float max_luminance, 
                                      float min_luminance, 
                                      int output_width) {

    float denominator = max_luminance - min_luminance;
    if (denominator == 0) {
        std::cout << "Image is homogenous, can't normalize" << endl;
        std::exit(1);
    }

    //std::cout << "denominator: " << denominator << endl;

    for (int index=0; index < cells.size(); index++) {

        if (cells[index].symbol == '\0') {

            float normalized_value = float(cells[index].luminance - min_luminance) / denominator;
            //std::cout << normalized_value;
            int ramp_index = normalized_value * 9;
            //std::cout << "ramp_index: "<< ramp_index << endl;
            
            char symbol = ramp[ramp_index];
            cells[index].symbol = symbol;
        }
    }
}

void apply_sobel_filter(std::vector<AsciiCell> &cells, int output_height, int output_width) {
    for (int row=0;row<output_height; row++) {
      for (int col=0;col<output_width; col++) {
        
        float gx = 0, gy = 0;

        for (int i=-1;i<2;i++) {
            for (int j=-1;j<2;j++) {
                int y_coord = std::clamp(row + i, 0, output_height-1);
                int x_coord = std::clamp(col + j, 0, output_width-1);

                float cell_value = cells[x_coord + output_width*(y_coord)].luminance;
                gx += cell_value * Gx[i+1][j+1];
                gy += cell_value * Gy[i+1][j+1];
            }
        }
        float G = sqrt(pow(gx, 2) + pow(gy, 2));
        if (G > EDGE_THRESHOLD) {
            float theta = atan2(-gy, gx);
            float edge_angle = theta - pi / 2.0f;

            char symbol;
            
            // rotate to [0, pi]
            while (edge_angle < 0) edge_angle += pi;

            if (edge_angle < pi/6 || edge_angle >= (5.0f / 6.0f) * pi) {
                symbol = '_';
            }
            else if (edge_angle < pi / 3) {
                symbol = '/';
            }
            else if (edge_angle < (2.0f / 3.0f) * pi) {
                symbol = '|';
            }
            else {
                symbol = '\\';
            }

            cells[col + output_width*(row)].symbol = symbol;
        }
      }
    }
}

void ascii_convert(unsigned char* image_buffer, std::vector<AsciiCell> &cells, int output_height, int output_width, int width, int height, int color_channels) {
    unsigned char r,g,b;
   
    float max_luminance = 0;
    float min_luminance = std::numeric_limits<int>::max();

    for (int row=0; row<output_height; row++) {
        for (int col=0; col<output_width; col++) {
            int i = row * height / output_height;
            int i_end = (row + 1) * height / output_height;

            int j = col * width / output_width;
            int j_end = (col + 1) * width / output_width;

            float sum_luminance = 0;

            int num_of_values = 0, sum_r = 0, sum_g = 0, sum_b = 0;

            for (i; i < i_end; i++) {
                for (j; j < j_end; j++) {

                r = image_buffer[i*width*color_channels + j*color_channels];
                g = image_buffer[i*width*color_channels + j*color_channels + 1];
                b = image_buffer[i*width*color_channels + j*color_channels + 2];
                
                sum_r += r;
                sum_g += g;
                sum_b+= b;

                num_of_values++;
                sum_luminance += perceived_luminance(r,g,b);
                }
            }

            float average_luminance = sum_luminance / num_of_values;
            cells.push_back(AsciiCell{average_luminance,
                                      '\0',
                                      sum_r / num_of_values,
                                      sum_g / num_of_values,
                                      sum_b / num_of_values});
            
            max_luminance = max(average_luminance, max_luminance);
            min_luminance = min(average_luminance, min_luminance);
        }
    }
    
    
    apply_sobel_filter(cells, output_height, output_width);
    normalize_values_and_assign_char(cells, max_luminance, min_luminance, output_width);
    
    return;
}

void save_svg(const std::vector<AsciiCell>& cells, int rows, int cols) {
    constexpr int char_width = 10;
    constexpr int char_height = 18;
    constexpr int font_size = 16;

    std::ofstream svg("ascii.svg");
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << cols * char_width << "\" "
        << "height=\"" << rows * char_height << "\" "
        << "viewBox=\"0 0 " << cols * char_width << " "
        << rows * char_height << "\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"#111\"/>\n";
    svg << "<g font-family=\"monospace\" font-size=\"" << font_size
        << "\" xml:space=\"preserve\">\n";

    for (int row=0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const AsciiCell& cell = cells[row * cols + col];
            svg << "<text x=\"" << col * char_width
                << "\" y=\"" << row * char_height + font_size
                << "\" fill=\"rgb(" << cell.r << "," << cell.g << ","
                << cell.b << ")\">" << cell.symbol << "</text>\n";
        }
    }

    svg << "</g>\n</svg>\n";
}

void print_to_terminal(std::vector<AsciiCell> &cells, int output_width) {
     for (int i = 0; i<cells.size(); i++){
        AsciiCell cell = cells[i]; 
        std::cout << "\x1b[38;2;" << cell.r << ';' << cell.g << ';' << cell.b << 'm' << cell.symbol << "\x1b[0m";

        if (!((i + 1) % output_width)) {
            std::cout << "\n";
        }
    }   
}

void image(MediaInfo &media_info) {
    
    unsigned char* image_buffer = stbi_load(media_info.filename.c_str(), &media_info.width, &media_info.height, &media_info.color_channels, 0);

    int output_height = static_cast<int>(media_info.height * (static_cast<double>(media_info.output_width) / media_info.width) * media_info.char_aspect_ratio);
    ascii_convert(
        image_buffer, 
        media_info.cells, 
        output_height, 
        media_info.output_width, 
        media_info.width, 
        media_info.height, 
        media_info.color_channels);
    
    stbi_image_free(image_buffer);

    print_to_terminal(media_info.cells, media_info.output_width);
    save_svg(media_info.cells, output_height, media_info.output_width);
}

void create_gif(MediaInfo &media_info) {
    std::ifstream file(media_info.filename, std::ios::binary);
        std::vector<stbi_uc> bytes{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
        
        int frame_count = 0;
        int *delays = nullptr;
        
        unsigned char* gif_buffer = stbi_load_gif_from_memory(
            bytes.data(),
            static_cast<int>(bytes.size()),
            &delays,
            &media_info.width,
            &media_info.height,
            &frame_count,
            &media_info.color_channels,
            4);
        
        int output_height = static_cast<int>(media_info.height * (static_cast<double>(media_info.output_width) / media_info.width) * media_info.char_aspect_ratio);

        while (true) {

            for (int i=0; i<frame_count; i++) {
                media_info.cells.clear();
                
                unsigned char* image_buffer = gif_buffer + i * media_info.width * media_info.height * 4; // 4 is the number of bytes per GIF pixel RGBA
                ascii_convert(
                    image_buffer, 
                    media_info.cells, 
                    output_height, 
                    media_info.output_width, 
                    media_info.width, 
                    media_info.height, 
                    media_info.color_channels);
                
                std::cout << "\033[2J\033[H";
                print_to_terminal(media_info.cells, media_info.output_width);
                std::cout.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        stbi_image_free(gif_buffer);
        stbi_image_free(delays);
}


int main(int argc, char** argv) {
    
    int width, height, color_channels;
    std::vector<AsciiCell> cells;

    std::string filename = argv[2];
    std::string output_width_str = argv[3];

    double char_aspect_ratio = 0.5;
    int output_width = std::stoi(output_width_str);

    MediaInfo media_info = MediaInfo{filename, cells, width, height, color_channels, output_width, char_aspect_ratio};
    if (std::string(argv[1]) == "--image") {
        image(media_info);
    }
    else if (std::string(argv[1]) == "--gif") {
        create_gif(media_info);
    }
   
    return 0;
}