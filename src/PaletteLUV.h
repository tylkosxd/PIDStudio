#pragma once

#include <memory>

const double xyzMatrix[3][3] = {
    {0.4124564, 0.3575761, 0.1804375},
    {0.2126729, 0.7151522, 0.0721750},
    {0.0193339, 0.1191920, 0.9503041}
};

typedef struct colorLUV {
    double L;
    double U;
    double V;
} colorLUV;

const double Xn = 0.950489;
const double Yn = 1.0;
const double Zn = 1.088840;

const double epsilon = 0.008856;
const double kappa = 903.3;

class PaletteLUV {
    public:
        PaletteLUV(unsigned char* rgbPaletteData);
        int getMostSimilarColor(int index, std::shared_ptr<PaletteLUV> otherPal);

    private:
        colorLUV getColor(int index);
        double data[256][3];
};
