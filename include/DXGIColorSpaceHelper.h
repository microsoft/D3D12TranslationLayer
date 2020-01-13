// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

// ----------------------------------------------------------------------------
//
// CDXGIColorSpaceHelper
//
// ----------------------------------------------------------------------------
class CDXGIColorSpaceHelper
{
public:
    //----------------------------------------------------------------------------------------------------------------------------------
    static bool IsRGBColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace)
    {
        switch (ColorSpace)
        {
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020:
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    static bool IsStudioColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace)
    {
        switch (ColorSpace)
        {
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020:
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    static bool Is709ColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace)
    {
        switch (ColorSpace)
        {
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709:
        case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709:
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    static bool Is2020ColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace)
    {
        switch (ColorSpace)
        {
        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020:
        case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020:
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020:
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    static bool Is601ColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace)
    {
        switch (ColorSpace)
        {
        case DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601:
        case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601:
        case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601:
            return true;
        }
        return false;
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    static DXGI_COLOR_SPACE_TYPE ConvertFromLegacyColorSpace(bool RGB, UINT BitsPerElement, bool StudioRGB, bool P709, bool StudioYUV)
    {
        if (RGB)
        {
            if (BitsPerElement > 32)
            {
                // All 16 bit color channel data is assumed to be linear rather than SRGB
                return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            }
            else
            {
                if (StudioRGB)
                {
                    return DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709;
                }
                else
                {
                    return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
                }
            }
        }
        else
        {
            if (P709)
            {
                if (StudioYUV)
                {
                    return DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
                }
                else
                {
                    return DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709;
                }
            }
            else
            {
                if (StudioYUV)
                {
                    return DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601;
                }
                else
                {
                    return DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601;
                }
            }
        }
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    static DXGI_COLOR_SPACE_TYPE GetDefaultColorSpaceFromFormat(bool RGB, UINT BitsPerElement)
    {
        if (RGB)
        {
            if (BitsPerElement > 32)
            {
                // All 16 bit color channel data is assumed to be linear rather than SRGB
                return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            }
            else
            {
                return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            }
        }
        else
        {
            return DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601;
        }
    }

};
