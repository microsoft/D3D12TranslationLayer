/*-------------------------------------------------------------------------------------
 *
 * Copyright (c) Microsoft Corporation
 *
 *-------------------------------------------------------------------------------------*/


/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0622 */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __d3d12compatibility_h__
#define __d3d12compatibility_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ID3D12CompatibilityDevice_FWD_DEFINED__
#define __ID3D12CompatibilityDevice_FWD_DEFINED__
typedef interface ID3D12CompatibilityDevice ID3D12CompatibilityDevice;

#endif 	/* __ID3D12CompatibilityDevice_FWD_DEFINED__ */


#ifndef __ID3D12CompatibilityQueue_FWD_DEFINED__
#define __ID3D12CompatibilityQueue_FWD_DEFINED__
typedef interface ID3D12CompatibilityQueue ID3D12CompatibilityQueue;

#endif 	/* __ID3D12CompatibilityQueue_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "d3d11on12.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_d3d12compatibility_0000_0000 */
/* [local] */ 

#include <winapifamily.h>
#pragma region Desktop Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
typedef 
enum D3D12_COMPATIBILITY_SHARED_FLAGS
    {
        D3D12_COMPATIBILITY_SHARED_FLAG_NONE	= 0,
        D3D12_COMPATIBILITY_SHARED_FLAG_NON_NT_HANDLE	= 0x1,
        D3D12_COMPATIBILITY_SHARED_FLAG_KEYED_MUTEX	= 0x2,
        D3D12_COMPATIBILITY_SHARED_FLAG_9_ON_12	= 0x4
    } 	D3D12_COMPATIBILITY_SHARED_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS( D3D12_COMPATIBILITY_SHARED_FLAGS );
typedef 
enum D3D12_REFLECT_SHARED_PROPERTY
    {
        D3D12_REFLECT_SHARED_PROPERTY_D3D11_RESOURCE_FLAGS	= 0,
        D3D12_REFELCT_SHARED_PROPERTY_COMPATIBILITY_SHARED_FLAGS	= ( D3D12_REFLECT_SHARED_PROPERTY_D3D11_RESOURCE_FLAGS + 1 ) ,
        D3D12_REFLECT_SHARED_PROPERTY_NON_NT_SHARED_HANDLE	= ( D3D12_REFELCT_SHARED_PROPERTY_COMPATIBILITY_SHARED_FLAGS + 1 ) 
    } 	D3D12_REFLECT_SHARED_PROPERTY;



extern RPC_IF_HANDLE __MIDL_itf_d3d12compatibility_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_d3d12compatibility_0000_0000_v0_0_s_ifspec;

#ifndef __ID3D12CompatibilityDevice_INTERFACE_DEFINED__
#define __ID3D12CompatibilityDevice_INTERFACE_DEFINED__

/* interface ID3D12CompatibilityDevice */
/* [unique][local][object][uuid] */ 


EXTERN_C const IID IID_ID3D12CompatibilityDevice;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8f1c0e3c-fae3-4a82-b098-bfe1708207ff")
    ID3D12CompatibilityDevice : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE CreateSharedResource( 
            _In_  const D3D12_HEAP_PROPERTIES *pHeapProperties,
            D3D12_HEAP_FLAGS HeapFlags,
            _In_  const D3D12_RESOURCE_DESC *pDesc,
            D3D12_RESOURCE_STATES InitialResourceState,
            _In_opt_  const D3D12_CLEAR_VALUE *pOptimizedClearValue,
            _In_opt_  const D3D11_RESOURCE_FLAGS *pFlags11,
            D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
            _In_opt_  ID3D12LifetimeTracker *pLifetimeTracker,
            _In_opt_  ID3D12SwapChainAssistant *pOwningSwapchain,
            REFIID riid,
            _COM_Outptr_opt_  void **ppResource) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateSharedHeap( 
            _In_  const D3D12_HEAP_DESC *pHeapDesc,
            D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
            REFIID riid,
            _COM_Outptr_opt_  void **ppHeap) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ReflectSharedProperties( 
            _In_  ID3D12Object *pHeapOrResource,
            D3D12_REFLECT_SHARED_PROPERTY ReflectType,
            _Out_writes_bytes_(DataSize)  void *pData,
            UINT DataSize) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ID3D12CompatibilityDeviceVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ID3D12CompatibilityDevice * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ID3D12CompatibilityDevice * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ID3D12CompatibilityDevice * This);
        
        HRESULT ( STDMETHODCALLTYPE *CreateSharedResource )( 
            ID3D12CompatibilityDevice * This,
            _In_  const D3D12_HEAP_PROPERTIES *pHeapProperties,
            D3D12_HEAP_FLAGS HeapFlags,
            _In_  const D3D12_RESOURCE_DESC *pDesc,
            D3D12_RESOURCE_STATES InitialResourceState,
            _In_opt_  const D3D12_CLEAR_VALUE *pOptimizedClearValue,
            _In_opt_  const D3D11_RESOURCE_FLAGS *pFlags11,
            D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
            _In_opt_  ID3D12LifetimeTracker *pLifetimeTracker,
            _In_opt_  ID3D12SwapChainAssistant *pOwningSwapchain,
            REFIID riid,
            _COM_Outptr_opt_  void **ppResource);
        
        HRESULT ( STDMETHODCALLTYPE *CreateSharedHeap )( 
            ID3D12CompatibilityDevice * This,
            _In_  const D3D12_HEAP_DESC *pHeapDesc,
            D3D12_COMPATIBILITY_SHARED_FLAGS CompatibilityFlags,
            REFIID riid,
            _COM_Outptr_opt_  void **ppHeap);
        
        HRESULT ( STDMETHODCALLTYPE *ReflectSharedProperties )( 
            ID3D12CompatibilityDevice * This,
            _In_  ID3D12Object *pHeapOrResource,
            D3D12_REFLECT_SHARED_PROPERTY ReflectType,
            _Out_writes_bytes_(DataSize)  void *pData,
            UINT DataSize);
        
        END_INTERFACE
    } ID3D12CompatibilityDeviceVtbl;

    interface ID3D12CompatibilityDevice
    {
        CONST_VTBL struct ID3D12CompatibilityDeviceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ID3D12CompatibilityDevice_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ID3D12CompatibilityDevice_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ID3D12CompatibilityDevice_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ID3D12CompatibilityDevice_CreateSharedResource(This,pHeapProperties,HeapFlags,pDesc,InitialResourceState,pOptimizedClearValue,pFlags11,CompatibilityFlags,pLifetimeTracker,pOwningSwapchain,riid,ppResource)	\
    ( (This)->lpVtbl -> CreateSharedResource(This,pHeapProperties,HeapFlags,pDesc,InitialResourceState,pOptimizedClearValue,pFlags11,CompatibilityFlags,pLifetimeTracker,pOwningSwapchain,riid,ppResource) ) 

#define ID3D12CompatibilityDevice_CreateSharedHeap(This,pHeapDesc,CompatibilityFlags,riid,ppHeap)	\
    ( (This)->lpVtbl -> CreateSharedHeap(This,pHeapDesc,CompatibilityFlags,riid,ppHeap) ) 

#define ID3D12CompatibilityDevice_ReflectSharedProperties(This,pHeapOrResource,ReflectType,pData,DataSize)	\
    ( (This)->lpVtbl -> ReflectSharedProperties(This,pHeapOrResource,ReflectType,pData,DataSize) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ID3D12CompatibilityDevice_INTERFACE_DEFINED__ */


#ifndef __ID3D12CompatibilityQueue_INTERFACE_DEFINED__
#define __ID3D12CompatibilityQueue_INTERFACE_DEFINED__

/* interface ID3D12CompatibilityQueue */
/* [unique][local][object][uuid] */ 


EXTERN_C const IID IID_ID3D12CompatibilityQueue;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7974c836-9520-4cda-8d43-d996622e8926")
    ID3D12CompatibilityQueue : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE AcquireKeyedMutex( 
            _In_  ID3D12Object *pHeapOrResourceWithKeyedMutex,
            UINT64 Key,
            DWORD dwTimeout,
            _Reserved_  void *pReserved,
            _In_range_(0,0)  UINT Reserved) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ReleaseKeyedMutex( 
            _In_  ID3D12Object *pHeapOrResourceWithKeyedMutex,
            UINT64 Key,
            _Reserved_  void *pReserved,
            _In_range_(0,0)  UINT Reserved) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ID3D12CompatibilityQueueVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ID3D12CompatibilityQueue * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ID3D12CompatibilityQueue * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ID3D12CompatibilityQueue * This);
        
        HRESULT ( STDMETHODCALLTYPE *AcquireKeyedMutex )( 
            ID3D12CompatibilityQueue * This,
            _In_  ID3D12Object *pHeapOrResourceWithKeyedMutex,
            UINT64 Key,
            DWORD dwTimeout,
            _Reserved_  void *pReserved,
            _In_range_(0,0)  UINT Reserved);
        
        HRESULT ( STDMETHODCALLTYPE *ReleaseKeyedMutex )( 
            ID3D12CompatibilityQueue * This,
            _In_  ID3D12Object *pHeapOrResourceWithKeyedMutex,
            UINT64 Key,
            _Reserved_  void *pReserved,
            _In_range_(0,0)  UINT Reserved);
        
        END_INTERFACE
    } ID3D12CompatibilityQueueVtbl;

    interface ID3D12CompatibilityQueue
    {
        CONST_VTBL struct ID3D12CompatibilityQueueVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ID3D12CompatibilityQueue_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ID3D12CompatibilityQueue_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ID3D12CompatibilityQueue_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ID3D12CompatibilityQueue_AcquireKeyedMutex(This,pHeapOrResourceWithKeyedMutex,Key,dwTimeout,pReserved,Reserved)	\
    ( (This)->lpVtbl -> AcquireKeyedMutex(This,pHeapOrResourceWithKeyedMutex,Key,dwTimeout,pReserved,Reserved) ) 

#define ID3D12CompatibilityQueue_ReleaseKeyedMutex(This,pHeapOrResourceWithKeyedMutex,Key,pReserved,Reserved)	\
    ( (This)->lpVtbl -> ReleaseKeyedMutex(This,pHeapOrResourceWithKeyedMutex,Key,pReserved,Reserved) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ID3D12CompatibilityQueue_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_d3d12compatibility_0000_0002 */
/* [local] */ 

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */
#pragma endregion
DEFINE_GUID(IID_ID3D12CompatibilityDevice,0x8f1c0e3c,0xfae3,0x4a82,0xb0,0x98,0xbf,0xe1,0x70,0x82,0x07,0xff);
DEFINE_GUID(IID_ID3D12CompatibilityQueue,0x7974c836,0x9520,0x4cda,0x8d,0x43,0xd9,0x96,0x62,0x2e,0x89,0x26);


extern RPC_IF_HANDLE __MIDL_itf_d3d12compatibility_0000_0002_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_d3d12compatibility_0000_0002_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


