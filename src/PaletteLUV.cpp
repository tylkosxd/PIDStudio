#include "PaletteLUV.h"
#include <cmath>

colorLUV createSingleColorLUV(uint8_t red, uint8_t green, uint8_t blue) {
    uint8_t RGB[3] = {red, green, blue};
    double linearRGB[3];
    double xyz[3];
    colorLUV LUV;

    auto rgbToLinear = [&]() {
        double linearChannel;
        for (int channel = 0; channel < 3; channel++) {
            linearChannel = (double)(RGB[channel])/255.0;
            if (linearChannel <= 0.04045)
                linearRGB[channel] = linearChannel/12.92;
            else
                linearRGB[channel] = std::powf((linearChannel + 0.055)/1.055, 2.4f);
        }
    };

    auto rgbLinearToXyz = [&]() {
        for (int channel = 0; channel < 3; channel++) {
            xyz[channel] = 
                xyzMatrix[channel][0]*linearRGB[0] + 
                xyzMatrix[channel][1]*linearRGB[1] + 
                xyzMatrix[channel][2]*linearRGB[2];
        }
    };

    rgbToLinear();

    rgbLinearToXyz();

    if (xyz[0] == 0 && xyz[1] == 0 && xyz[2] == 0) {
        LUV.L = 0.0;
        LUV.U = 0.0;
        LUV.V = 0.0;
        return LUV;
    }
    LUV.L = xyz[1] > epsilon ? (116 * std::cbrtf(xyz[1])) : (kappa * xyz[1]);
    double up = 4 * xyz[0] / (xyz[0] + 15*xyz[1] + 3*xyz[2]);
    double urp = 4 * Xn / (Xn + 15*Yn + 3*Zn);
    LUV.U = 13 * LUV.L * (up - urp);
    double vp = 9 * xyz[1] / (xyz[0] + 15*xyz[1] + 3*xyz[2]);
    double vrp = 9 * Yn / (Xn + 15*Yn + 3*Zn);
    LUV.V = 13 * LUV.L * (vp - vrp);
    return LUV;
}

PaletteLUV::PaletteLUV(unsigned char* rgbPaletteData) {
    unsigned char* currentRGB;

    for (int entry = 0; entry < 256; entry++){
        currentRGB = rgbPaletteData + entry*3;
        data[entry] = createSingleColorLUV(currentRGB[0], currentRGB[1], currentRGB[2]);
    }
}

colorLUV PaletteLUV::getColor(int index) {
    return data[index];
}

int PaletteLUV::getMostSimilarColor(int index, const std::unique_ptr<PaletteLUV>& otherPal) {
    if (index < 1 || index > 255) return 0;

    float distance; /* square of euclidean distance */
    std::pair<int, float> result = {0, FLT_MAX}; /* {palette index, smallest distance} - for the visually the most similar color */
    colorLUV thisColor = getColor(index);
    colorLUV otherColor;

    for (int i = 1; i < 256; i++) { /* omitting index 0 */
        otherColor = otherPal -> getColor(i);
        distance = std::pow((thisColor.L - otherColor.L), 2) +
            std::pow((thisColor.U - otherColor.U), 2) +
            std::pow((thisColor.V - otherColor.V), 2); /* don't need to square root it */
        if (distance < result.second) {
            result.first = i;
            result.second = distance;
        }
    }

    return result.first;
}

uint8_t PaletteLUV::getMostSimilarColor(uint8_t red, uint8_t green, uint8_t blue) {
    colorLUV thisColor = createSingleColorLUV(red, green, blue);

    colorLUV palColor;
    float distance;
    std::pair<int, float> result = {0, FLT_MAX}; /* {palette index, smallest distance} - for the visually the most similar color */

    for (int i = 1; i < 256; i++) { /* continue the same for other colors */
        palColor = getColor(i);
        distance = std::pow((thisColor.L - palColor.L), 2) +
            std::pow((thisColor.U - palColor.U), 2) +
            std::pow((thisColor.V - palColor.V), 2); /* square of euclidean distance, do not root it */
        if (distance < result.second ) {
            result.first = i;
            result.second = distance;
        }
    }

    return result.first;
}