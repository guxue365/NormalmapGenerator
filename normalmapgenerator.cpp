#include "normalmapgenerator.h"
#include <QVector3D>
#include <QColor>

NormalmapGenerator::NormalmapGenerator(IntensityMap::Mode mode, bool useRed, bool useGreen, bool useBlue, bool useAlpha)
{
    this->mode = mode;
    this->useRed = useRed;
    this->useGreen = useGreen;
    this->useBlue = useBlue;
    this->useAlpha = useAlpha;
}

QImage NormalmapGenerator::calculateNormalmap(QImage input, Kernel kernel, double strength, bool invert, bool tileable) {
    this->tileable = tileable;

    this->intensity = IntensityMap(input, mode, useRed, useGreen, useBlue, useAlpha);
    if(invert)
        intensity.invert();

    QImage result(input.width(), input.height(), QImage::Format_ARGB32);

    #pragma omp parallel for  // OpenMP
    //code from http://stackoverflow.com/a/2368794
    for(int y = 0; y < input.height(); y++) {
        for(int x = 0; x < input.width(); x++) {

            double topLeft      = intensity.at(handleEdges(x - 1, input.width()), handleEdges(y - 1, input.height()));
            double top          = intensity.at(handleEdges(x - 1, input.width()), handleEdges(y,     input.height()));
            double topRight     = intensity.at(handleEdges(x - 1, input.width()), handleEdges(y + 1, input.height()));
            double right        = intensity.at(handleEdges(x,     input.width()), handleEdges(y + 1, input.height()));
            double bottomRight  = intensity.at(handleEdges(x + 1, input.width()), handleEdges(y + 1, input.height()));
            double bottom       = intensity.at(handleEdges(x + 1, input.width()), handleEdges(y,     input.height()));
            double bottomLeft   = intensity.at(handleEdges(x + 1, input.width()), handleEdges(y - 1, input.height()));
            double left         = intensity.at(handleEdges(x,     input.width()), handleEdges(y - 1, input.height()));

            double convolution_kernel[3][3] = {{topLeft, top, topRight},
                                               {left, 0.0, right},
                                               {bottomLeft, bottom, bottomRight}};

            //sobel filter (original implementation copied from stackoverflow)
            //double dY = (topRight + 2.0 * right + bottomRight) - (topLeft + 2.0 * left + bottomLeft);
            //double dX = (bottomLeft + 2.0 * bottom + bottomRight) - (topLeft + 2.0 * top + topRight);
            //double dZ = 1.0 / strength;

            //QVector3D normal(dX, dY, dZ);
            //normal.normalize();

            QVector3D normal(0, 0, 0);

            if(kernel == SOBEL)
                normal = sobel(convolution_kernel, strength);
            else if(kernel == PREWITT)
                normal = prewitt(convolution_kernel, strength);

            QColor normalAsRgb(mapComponent(normal.x()), mapComponent(normal.y()), mapComponent(normal.z()));
            result.setPixel(x, y, normalAsRgb.rgb());
        }
    }

    return result;
}

QVector3D NormalmapGenerator::sobel(double convolution_kernel[3][3], double strength) {
    double top_side    = convolution_kernel[0][0] + 2.0 * convolution_kernel[0][1] + convolution_kernel[0][2];
    double bottom_side = convolution_kernel[2][0] + 2.0 * convolution_kernel[2][1] + convolution_kernel[2][2];
    double right_side  = convolution_kernel[0][2] + 2.0 * convolution_kernel[1][2] + convolution_kernel[2][2];
    double left_side   = convolution_kernel[0][0] + 2.0 * convolution_kernel[1][0] + convolution_kernel[2][0];

    double dY = right_side - left_side;
    double dX = bottom_side - top_side;
    double dZ = 1.0 / strength;

    return QVector3D(dX, dY, dZ).normalized();
}

QVector3D NormalmapGenerator::prewitt(double convolution_kernel[3][3], double strength) {
    double top_side    = convolution_kernel[0][0] + convolution_kernel[0][1] + convolution_kernel[0][2];
    double bottom_side = convolution_kernel[2][0] + convolution_kernel[2][1] + convolution_kernel[2][2];
    double right_side  = convolution_kernel[0][2] + convolution_kernel[1][2] + convolution_kernel[2][2];
    double left_side   = convolution_kernel[0][0] + convolution_kernel[1][0] + convolution_kernel[2][0];

    double dY = right_side - left_side;
    double dX = top_side - bottom_side;
    double dZ = 1.0 / strength;

    return QVector3D(dX, dY, dZ).normalized();
}

int NormalmapGenerator::handleEdges(int iterator, int max) {
    if(iterator >= max) {
        //move iterator from end to beginning + overhead
        if(tileable)
            return max - iterator;
        else
            return max - 1;
    }
    else if(iterator < 0) {
        //move iterator from beginning to end - overhead
        if(tileable)
            return max + iterator;
        else
            return 0;
    }
    else {
        return iterator;
    }
}

//transform -1..1 to 0..255
int NormalmapGenerator::mapComponent(double value) {
    return (value + 1.0) * (255.0 / 2.0);
}
