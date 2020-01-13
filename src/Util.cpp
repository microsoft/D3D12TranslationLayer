// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#include "pch.h"


namespace D3D12TranslationLayer
{
    UINT GetByteAlignment(DXGI_FORMAT format)
    {
        return CD3D11FormatHelper::GetByteAlignment(format);
    }
}

#ifndef NO_IMPLEMENT_RECT_FNS
#pragma warning(disable: 4273) // Inconsistent DLL linkage - this is by design

// Avoid linking in the user32 dependency
BOOL APIENTRY IntersectRect(
    __out LPRECT prcDst,
    __in CONST RECT *prcSrc1,
    __in CONST RECT *prcSrc2)

{
    prcDst->left = max(prcSrc1->left, prcSrc2->left);
    prcDst->right = min(prcSrc1->right, prcSrc2->right);

    /*
     * check for empty rect
     */
    if (prcDst->left < prcDst->right) {

        prcDst->top = max(prcSrc1->top, prcSrc2->top);
        prcDst->bottom = min(prcSrc1->bottom, prcSrc2->bottom);

        /*
         * check for empty rect
         */
        if (prcDst->top < prcDst->bottom) {
            return TRUE;        // not empty
        }
    }

    /*
     * empty rect
     */
    *prcDst = {};

    return FALSE;
}

BOOL APIENTRY UnionRect(
    __out LPRECT prcDst,
    __in CONST RECT *prcSrc1,
    __in CONST RECT *prcSrc2)
{
    BOOL frc1Empty, frc2Empty;

    frc1Empty = ((prcSrc1->left >= prcSrc1->right) ||
        (prcSrc1->top >= prcSrc1->bottom));

    frc2Empty = ((prcSrc2->left >= prcSrc2->right) ||
        (prcSrc2->top >= prcSrc2->bottom));

    if (frc1Empty && frc2Empty) {
        *prcDst = {};
        return FALSE;
    }

    if (frc1Empty) {
        *prcDst = *prcSrc2;
        return TRUE;
    }

    if (frc2Empty) {
        *prcDst = *prcSrc1;
        return TRUE;
    }

    /*
     * form the union of the two non-empty rects
     */
    prcDst->left = min(prcSrc1->left, prcSrc2->left);
    prcDst->top = min(prcSrc1->top, prcSrc2->top);
    prcDst->right = max(prcSrc1->right, prcSrc2->right);
    prcDst->bottom = max(prcSrc1->bottom, prcSrc2->bottom);

    return TRUE;
}
#endif