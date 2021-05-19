#pragma once
#include <BlobContainer.h>

//=================================================================================================================================
// CDXBCBuilder
//
// Create, fill and retrieve a DXBC (DX Blob Container).
//
// Basic usage:
// (1) Create CDXBCBuilder class instance using constructor (takes a parameter regarding whether to copy or allocate)
// (2) For each blob of data you want to store, call AppendBlob()
// (4) Finally, call either GetFinalDXBC() to retrieve the DXBC.
// (5) You can start over again by calling StartNewContainer(), or just get rid of the class.
//
// Read comments inline below for full detail.
//
class CDXBCBuilder
{
public:
    // Constructor
    CDXBCBuilder(bool bMakeInternalCopiesOfBlobs)
    {
        // Setting bMakeInternalCopiesOfBlobs = true means that while the container is being built up
        // and you are passing in pointers to your blob data via AppendBlob(), this class will
        // allocate and make copies of the blobs you pass in.
        // Setting bMakeInternalCopiesOfBlobs = false means that AppendBlob() will store the
        // pointers you pass in, rather than copying the data.  This is more efficient than
        // copying the data, but can only be used if you know that up until you finish building
        // the container by calling GetFinalDXBC*(), the pointers will continue to point to valid data.
        //
        // Note that the actual DXBC is only built up at the call to GetFinalDXBC(), when
        // all the blobs are traversed and copied into one contiguous memory allocation (the DXBC),
        // regardless of whether bMakeInternalCopiesOfBlobs is true or false.
        m_bMakeInternalCopiesOfBlobs = bMakeInternalCopiesOfBlobs;
        Init();
    }
    ~CDXBCBuilder() { Cleanup(); }

    // Call to begin a new container.  Don't need to call this the first time (constructor already sets it up).
    void StartNewContainer();

    // Once a container has been started, use AppendBlob to append blobs of data to the container.
    // Each blob needs a fourCC to identify it (nothing wrong with adding multiple
    // blobs with the same fourCC though; they'll all be stored). Valid FourCCs come from the
    // enumeration near the top of this file.
    // The memory for the contents of the blob is copied into internally allocated storage
    // if the class was created with bMakeInternalCopiesOfBlobs, else direct pointers to
    // the data passed in are stored.
    // It is valid for a blob to be completely empty (BlobSize = 0)
    // Returns:
    //   S_OK           - On success.
    //   E_OUTOFMEMORY  - When internal memory allocation failed.
    //   E_FAIL         - If the total size of the blobs is beyond what is allowed,
    //                    or a container hasn't been started, or (BlobSize > 0 && !pBlobData),
    //                    or the BlobFourCC isn't recognized.
    HRESULT AppendBlob(DXBCFourCC BlobFourCC, UINT32 BlobSizeInBytes, const void *pBlobData);

    // Grabs a blob out of the specified parser and appends it to this builder
    // Returns: S_OK, S_FALSE (could not find blob), E_OUTOFMEMORY
    HRESULT AppendBlob(CDXBCParser *pParser, DXBCFourCC BlobFourCC);

    // After all blobs have been added, call GetFinalDXBC with pCallerAllocatedMemory set to NULL
    // to retrieve the required memory size for the final blob (output to pContainerSize).
    // Allocate that memory yourself, call GetFinalDXBC again passing in the memory and how much
    // was allocated, and the final container will be written out to the memory passed in, as long as it
    // is big enough, else nothing will be written).
    // When data is written out, the amount written is placed in pContainerSize if provided (0 if there
    // wasn't enough space, else the full size).
    //
    // Return values: S_OK, E_FAIL or E_OUTOFMEMORY (MS API hash algorithm code could run out of mem)

    HRESULT GetFinalDXBC(void *pCallerAllocatedMemory, UINT32 *pContainerSize);

private:
    typedef struct BlobNode
    {
        DXBCBlobHeader BlobHeader;
        const void *pBlobData;
        BlobNode *pNext;
    } BlobNode;
    bool m_bMakeInternalCopiesOfBlobs;
    UINT32 m_TotalOutputContainerSize; // to check against DXBC_MAX_SIZE_IN_BYTES
    UINT32 m_BlobCount;
    BlobNode *m_pFirstBlob;
    BlobNode *m_pLastBlob;
    void Init();
    void Cleanup();
};