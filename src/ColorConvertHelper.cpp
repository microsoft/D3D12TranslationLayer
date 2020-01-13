// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include <dxva.h>
#include <intsafe.h>

namespace D3D12TranslationLayer
{
    typedef enum {
        PRIMARY_709,
        PRIMARY_2020,
        PRIMARY_601,
        PRIMARY_UNDEFINED
    } PrimaryType;

    typedef float CCMatrix[3][3];

    typedef enum {
        NOMINAL_RANGE_TYPE_FULL,
        NOMINAL_RANGE_TYPE_STUDIO,
    } NominalRangeType;

    typedef struct {
        float YRange[2];
        float CRange[2];
    } CCNominalRange;


    // for RGB to YCbCr (same primaries), assumes normalized input
    static const CCMatrix RGBToYCbCr[] =
    {
        {
            // PRIMARY_709,
            { 0.212600f,  0.715200f,  0.072200f },
            { -0.114572f, -0.385428f,  0.500000f },
            { 0.500000f, -0.454153f, -0.045847f },
        },
        {
            // PRIMARY_2020,
            { 0.262700f,  0.678000f,  0.059300f },
            { -0.139630f, -0.360370f,  0.500000f },
            { 0.500000f, -0.459786f, -0.040214f },
        },
        {
            // PRIMARY_601,
            { 0.299000f,  0.587000f,  0.114000f },
            { -0.168736f, -0.331264f,  0.500000f },
            { 0.500000f, -0.418688f, -0.081312f },
        },
    };

    // for YCbCr to RGB (same primaries), assumes normalized input
    static const CCMatrix YCbCrToRGB[] =
    {
        // PRIMARY_709 
        {
            { 1.000000f,  0.000000f,  1.574800f },
            { 1.000000f, -0.187324f, -0.468124f },
            { 1.000000f,  1.855600f,  0.000000f },
        },
        // PRIMARY_2020 
        {
            { 1.000000f, -0.000000f,  1.474600f },
            { 1.000000f, -0.164553f, -0.571353f },
            { 1.000000f,  1.881400f,  0.000000f },
        },
        // PRIMARY_601
        {
            { 1.000000f,  0.000000f,  1.402000f },
            { 1.000000f, -0.344136f, -0.714136f },
            { 1.000000f,  1.772000f, -0.000000f },
        },
    };

    static const CCNominalRange CCNominalRanges[] = {
        {
            // NOMINAL_RANGE_TYPE_FULL,
            { 0.0f, 1.0f },
            { 0.0f, 1.0f },
        },
        {
            // NOMINAL_RANGE_TYPE_STUDIO,
            { 0.063f, 0.92f },
            { 0.063f, 0.94f },
        },
    };

    static PrimaryType GetPrimaryType(DXGI_COLOR_SPACE_TYPE colorSpace)
    {
        if (CDXGIColorSpaceHelper::Is709ColorSpace(colorSpace))
        {
            return PRIMARY_709;
        }
        else if (CDXGIColorSpaceHelper::Is2020ColorSpace(colorSpace))
        {
            return PRIMARY_2020;
        }
        else if (CDXGIColorSpaceHelper::Is601ColorSpace(colorSpace))
        {
            return PRIMARY_601;
        }
        else
        {
           ThrowFailure(E_UNEXPECTED);     // need to handle new color spaces
        }
        return PRIMARY_UNDEFINED;
    }

    static NominalRangeType GetNominalRangeType(DXGI_COLOR_SPACE_TYPE colorSpace)
    {
        if (CDXGIColorSpaceHelper::IsStudioColorSpace(colorSpace))
        {
            return NOMINAL_RANGE_TYPE_STUDIO;
        }
        else
        {
            return NOMINAL_RANGE_TYPE_FULL;
        }
    }

    static float clip(float low, float high, float val)
    {
        return (val > high ? high : (val < low ? low : val));
    }

    static void mulmatrix(_In_ const CCMatrix& matrix, _In_reads_(4) const FLOAT normInput[4], _Out_writes_(4) FLOAT normOutput[4])
    {
        normOutput[0] = matrix[0][0] * normInput[0] + matrix[0][1] * normInput[1] + matrix[0][2] * normInput[2];
        normOutput[1] = matrix[1][0] * normInput[0] + matrix[1][1] * normInput[1] + matrix[1][2] * normInput[2];
        normOutput[2] = matrix[2][0] * normInput[0] + matrix[2][1] * normInput[1] + matrix[2][2] * normInput[2];
    }

    //
    // Implements a narrow subset of color conversions: full input nominal ranges and just does conversion from RGB to YUV and from YUV to RGB.
    //
    _Use_decl_annotations_
    void ColorConvertNormalized(const FLOAT normInput[4], DXGI_COLOR_SPACE_TYPE inputColorSpace, FLOAT normOutput[4], DXGI_COLOR_SPACE_TYPE outputColorSpace)
    {
        PrimaryType outputPrimary = GetPrimaryType(outputColorSpace);
        NominalRangeType outputNominalRange = GetNominalRangeType(outputColorSpace);
        bool inputRGB = CDXGIColorSpaceHelper::IsRGBColorSpace(inputColorSpace);
        bool outputRGB = CDXGIColorSpaceHelper::IsRGBColorSpace(outputColorSpace);

        if (inputRGB == outputRGB)
        {
            // this just converts from YUV to RGB and from RGB to YUV...
            ThrowFailure(E_INVALIDARG);
        }

        FLOAT input[4];
        input[0] = normInput[0];
        input[1] = normInput[1];
        input[2] = normInput[2];
        input[3] = normInput[3];

        // convert input from studio to full if needed
        if (CDXGIColorSpaceHelper::IsStudioColorSpace(inputColorSpace))
        {
            const CCNominalRange &nominalRange = CCNominalRanges[NOMINAL_RANGE_TYPE_STUDIO];
            const float YRange = nominalRange.YRange[1] - nominalRange.YRange[0];
            const float CRange = inputRGB ? YRange : nominalRange.CRange[1] - nominalRange.CRange[0];           // no difference from C/Y range if RGB
            const float CRange0 = inputRGB ? nominalRange.YRange[0] : nominalRange.CRange[0];

            // expand to full range and clip
            input[0] = (input[0] - nominalRange.YRange[0] ) / YRange;
            input[1] = (input[1] - CRange0) / CRange;
            input[2] = (input[2] - CRange0) / CRange;

            input[0] = clip(0.0f, 1.0f, input[0]);
            input[1] = clip(0.0f, 1.0f, input[1]);
            input[2] = clip(0.0f, 1.0f, input[2]);
        }

        // converts from full range into full range
        if (inputRGB)
        {
            mulmatrix(RGBToYCbCr[outputPrimary], input, normOutput);
            normOutput[1] += 0.5f;
            normOutput[2] += 0.5f;
        }
        else
        {
            input[1] = input[1] - 0.5f;
            input[2] = input[2] - 0.5f;
            mulmatrix(YCbCrToRGB[outputPrimary], input, normOutput);
        }
        normOutput[3] = input[3];

        // clip to 0->1 range
        normOutput[0] = clip(0.0f, 1.0f, normOutput[0]);
        normOutput[1] = clip(0.0f, 1.0f, normOutput[1]);
        normOutput[2] = clip(0.0f, 1.0f, normOutput[2]);

        // if the output is not full, we need to compress to studio range
        if (outputNominalRange != NOMINAL_RANGE_TYPE_FULL)
        {
            const CCNominalRange &nominalRange = CCNominalRanges[outputNominalRange];
            const float YRange = nominalRange.YRange[1] - nominalRange.YRange[0];
            const float CRange = outputRGB ? YRange : nominalRange.CRange[1] - nominalRange.CRange[0];   // no difference from C/Y range if RGB
            const float CRange0 = outputRGB ? nominalRange.YRange[0] : nominalRange.CRange[0];           // no difference from C/Y range if RGB
            const float CRange1 = outputRGB ? nominalRange.YRange[1] : nominalRange.CRange[1];           // no difference from C/Y range if RGB

            normOutput[0] = normOutput[0] * YRange + nominalRange.YRange[0];
            normOutput[1] = normOutput[1] * CRange + CRange0;
            normOutput[2] = normOutput[2] * CRange + CRange0;

            normOutput[0] = clip(nominalRange.YRange[0], nominalRange.YRange[1], normOutput[0]);
            normOutput[1] = clip(CRange0, CRange1, normOutput[1]);
            normOutput[2] = clip(CRange0, CRange1, normOutput[2]);
        }

    }
};