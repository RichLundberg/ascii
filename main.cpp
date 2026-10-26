#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

using namespace std;

constexpr char ramp[] = " .:-=+*#%@";
constexpr int edge_threshold = 80;
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
        if (G > edge_threshold) {
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

void ascii_convert(unsigned char* image_buffer,  std::vector<AsciiCell> &cells, int output_height, int output_width, int width, int height, int color_channels) {
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



int main(int argc, char** argv) {
    int width, height, color_channels;
    
    std::string filename = argv[1];
    std::string output_width_str = argv[2];
    std::vector<AsciiCell> cells;
    
    unsigned char* image_buffer = stbi_load(filename.c_str(), &width, &height, &color_channels, 0);

    double charAspectRatio = 0.5;
    int output_width = std::stoi(output_width_str);
    int output_height = static_cast<int>(height * (static_cast<double>(output_width) / width) * charAspectRatio);

    ascii_convert(image_buffer, cells, output_height, output_width, width, height, color_channels);
    
    stbi_image_free(image_buffer);

    for (int i = 0; i<cells.size(); i++){
        AsciiCell cell = cells[i]; 
        //38;2 to set character instead of background
        std::cout << "\x1b[38;2;" << cell.r << ';' << cell.g << ';' << cell.b << 'm' << cell.symbol << "\x1b[0m";

        if (!((i + 1) % output_width)) {
            std::cout << "\n";
        }
    }   
    return 0;
}