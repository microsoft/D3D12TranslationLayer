// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "BlobContainer.h"
#include "DxbcBuilder.hpp"

//=================================================================================================================================
// CDXBCBuilder

//---------------------------------------------------------------------------------------------------------------------------------
// CDXBCBuilder::Init()
void CDXBCBuilder::Init()
{
    m_TotalOutputContainerSize = sizeof(DXBCHeader);
    m_pFirstBlob = NULL;
    m_pLastBlob = NULL;
    m_BlobCount = 0;
}

//---------------------------------------------------------------------------------------------------------------------------------
// CDXBCBuilder::Cleanup()
void CDXBCBuilder::Cleanup()
{
    BlobNode *pNextBlob = m_pFirstBlob;
    while (pNextBlob)
    {
        BlobNode *pDeleteMe = pNextBlob;
        pNextBlob = pNextBlob->pNext;
        if (m_bMakeInternalCopiesOfBlobs)
        {
            free((void *)pDeleteMe->pBlobData);
        }
        free(pDeleteMe);
    }
    m_pFirstBlob = NULL;
    m_pLastBlob = NULL;
    m_TotalOutputContainerSize = 0;
    m_BlobCount = 0;
}

//---------------------------------------------------------------------------------------------------------------------------------
// CDXBCBuilder::StartNewContainer
void CDXBCBuilder::StartNewContainer()
{
    Cleanup();
    Init();
}

//---------------------------------------------------------------------------------------------------------------------------------
// CDXBCBuilder::AppendBlob
HRESULT CDXBCBuilder::AppendBlob(DXBCFourCC BlobFourCC, UINT BlobSize, const void *pBlobData)
{
    if ((BlobSize > 0 && !pBlobData))
    {
        return E_FAIL;
    }
    BlobNode *pNewBlobNode = (BlobNode *)malloc(sizeof(BlobNode));
    if (!pNewBlobNode)
    {
        return E_OUTOFMEMORY;
    }
    // Initialize node

    // Check what the new total output container size will be.
    UINT NewTotalSize = m_TotalOutputContainerSize + BlobSize +
                        4 /*container index entry*/ + sizeof(DXBCBlobHeader) /* blob header */;

    // Checked builds include extra debug info and so can be much larger,
    // so don't enforce the retail blob size limit.
#if DBG
    if ((NewTotalSize < m_TotalOutputContainerSize)) // overflow (wrap)
#else
    if ((NewTotalSize > DXBC_MAX_SIZE_IN_BYTES) ||   // overflow
        (NewTotalSize < m_TotalOutputContainerSize)) // overflow (wrap)
#endif
    {
        free(pNewBlobNode);
        return E_FAIL;
    }

    pNewBlobNode->BlobHeader.BlobFourCC = BlobFourCC;
    pNewBlobNode->BlobHeader.BlobSize = BlobSize;
    pNewBlobNode->pNext = NULL;
    if (BlobSize == 0)
    {
        pNewBlobNode->pBlobData = NULL;
    }
    else
    {
        if (m_bMakeInternalCopiesOfBlobs)
        {
            pNewBlobNode->pBlobData = (BlobNode *)malloc(BlobSize * sizeof(BYTE));
            if (!pNewBlobNode->pBlobData)
            {
                free(pNewBlobNode);
                return E_OUTOFMEMORY;
            }
            // Copy the blob data
            memcpy((BYTE *)pNewBlobNode->pBlobData, pBlobData, BlobSize);
        }
        else
        {
            pNewBlobNode->pBlobData = pBlobData;
        }
    }

    // Blob is valid, add new node to list
    m_TotalOutputContainerSize = NewTotalSize;
    m_BlobCount++;
    if (m_pLastBlob)
    {
        m_pLastBlob->pNext = pNewBlobNode;
    }
    if (!m_pFirstBlob)
    {
        m_pFirstBlob = pNewBlobNode;
    }
    m_pLastBlob = pNewBlobNode;
    return S_OK;
}

//---------------------------------------------------------------------------------------------------------------------------------
// CDXBCBuilder::AppendBlob
HRESULT CDXBCBuilder::AppendBlob(CDXBCParser *pParser, DXBCFourCC BlobFourCC)
{
    if (!pParser)
        return E_FAIL;

    UINT blobIndex = pParser->FindNextMatchingBlob(BlobFourCC);
    if (blobIndex == DXBC_BLOB_NOT_FOUND)
        return S_FALSE;

    const void *pData = pParser->GetBlob(blobIndex);
    UINT cbData = pParser->GetBlobSize(blobIndex);

    if (!pData)
        return E_FAIL;

    return AppendBlob(BlobFourCC, cbData, pData);
}

//---------------------------------------------------------------------------------------------------------------------------------
// CDXBCBuilder::GetFinalDXBC
HRESULT CDXBCBuilder::GetFinalDXBC(void *pCallerAllocatedMemory, UINT *pContainerSize)
{
    if (!pCallerAllocatedMemory)
    {
        // Return how much memory the caller needs to allocate.
        if (pContainerSize)
        {
            *pContainerSize = m_TotalOutputContainerSize;
            return S_OK;
        }
        return E_FAIL;
    }
    if (!pContainerSize)
    {
        return E_FAIL; // Nothing to do.
    }
    if (*pContainerSize < m_TotalOutputContainerSize)
    {
        // not enough memory allocated, return 0 bytes written out.
        *pContainerSize = 0;
        return E_FAIL;
    }
    // Ok, we can write out the full container.
    DXBCHeader *pHeader = (DXBCHeader *)pCallerAllocatedMemory;
    UINT *pIndex = (UINT *)((BYTE *)pHeader + sizeof(DXBCHeader));                                     // skip past header
    DXBCBlobHeader *pNextBlobHeader = (DXBCBlobHeader *)((BYTE *)pIndex + m_BlobCount * sizeof(UINT)); // skip past index
    BYTE *pNextBlobData = (BYTE *)pNextBlobHeader + sizeof(DXBCBlobHeader);                            // skip past blob header
    BlobNode *pNextInputBlob = m_pFirstBlob;
    for (UINT b = 0; b < m_BlobCount; b++)
    {
        pIndex[b] = (UINT)((BYTE *)pNextBlobHeader - (BYTE *)pCallerAllocatedMemory);
        *pNextBlobHeader = pNextInputBlob->BlobHeader;
        memcpy(pNextBlobData, pNextInputBlob->pBlobData, pNextInputBlob->BlobHeader.BlobSize);

        // advance to next blob
        pNextBlobHeader = (DXBCBlobHeader *)(pNextBlobData + pNextInputBlob->BlobHeader.BlobSize); // skip past last blob's data
        pNextBlobData = (BYTE *)pNextBlobHeader + sizeof(DXBCBlobHeader);                          // skip past blob header
        pNextInputBlob = pNextInputBlob->pNext;
    }

    // Fill in the initial entries
    pHeader->BlobCount = m_BlobCount;
    pHeader->ContainerSizeInBytes = m_TotalOutputContainerSize;
    pHeader->DXBCHeaderFourCC = DXBC_FOURCC_NAME;
    pHeader->Version.Major = DXBC_MAJOR_VERSION;
    pHeader->Version.Minor = DXBC_MINOR_VERSION;

    //signing is left as a post processing step if needed
    return S_OK;
}