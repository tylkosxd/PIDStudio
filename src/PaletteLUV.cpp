#include "PaletteLUV.h"
#include <cmath>

PaletteLUV::PaletteLUV(unsigned char* rgbPaletteData) {

    unsigned char* currentRGB;
    double linearRGB[3];
    double xyz[3];

    auto rgbToLinear = [&]() {
        double linearChannel;
        for (int channel = 0; channel < 3; channel++) {
            linearChannel = (double)(currentRGB[channel])/255;
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

    for (int entry = 0; entry < 256; entry++){
        currentRGB = rgbPaletteData + entry*3;

        rgbToLinear();

        rgbLinearToXyz();

        if (xyz[0] == 0 && xyz[1] == 0 && xyz[2] == 0) {
            data[entry][0] = 0; data[entry][1] = 0; data[entry][2] = 0;
            continue;
        }
        data[entry][0] = xyz[1] > epsilon ? (116 * std::cbrtf(xyz[1])) : (kappa * xyz[1]);
        double up = 4 * xyz[0] / (xyz[0] + 15*xyz[1] + 3*xyz[2]);
        double urp = 4 * Xn / (Xn + 15*Yn + 3*Zn);
        data[entry][1] = 13 * data[entry][0] * (up - urp);
        double vp = 9 * xyz[1] / (xyz[0] + 15*xyz[1] + 3*xyz[2]);
        double vrp = 9 * Yn / (Xn + 15*Yn + 3*Zn);
        data[entry][2] = 13 * data[entry][0] * (vp - vrp);
    }
}

colorLUV PaletteLUV::getColor(int index) {
    if (index < 0 || index > 255) return {0, 0, 0};
    return {data[index][0], data[index][1], data[index][2]};
}

int PaletteLUV::getMostSimilarColor(int index, std::shared_ptr<PaletteLUV> otherPal) {
    if (index < 1 || index > 255) return 0;

    double distance; /* square of euclidean distance */
    std::pair<int, double> result = {0, 0}; /* {palette index, smallest distance} - for the visually the most similar color */
    colorLUV thisColor = getColor(index);
    colorLUV otherColor;

    for (int i = 1; i < 256; i++) { /* omitting index 0 */
        otherColor = otherPal -> getColor(i);
        distance = std::pow((thisColor.L - otherColor.L), 2) +
            std::pow((thisColor.U - otherColor.U), 2) +
            std::pow((thisColor.V - otherColor.V), 2); /* don't need to square root it */
        if (!result.first || distance < result.second ) {
            result.first = i;
            result.second = distance;
        }
    }

    return result.first;
}