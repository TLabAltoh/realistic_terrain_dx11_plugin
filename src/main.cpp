#define _WIN32_WINNT 0x600
#define _PYBIND 1
#define _OUTPUTLOG 0
#define _TEST 0
#define SAFE_RELEASE(x) if( x != NULL ){ x->Release(); x = NULL; }

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <d3d11.h>
#include <d3dcompiler.h>

// Add
#include <tchar.h>
#include <iterator>
#include <iostream>
#include <list>
#include <cmath>
#include <random>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

namespace py = pybind11;

int m_mapSize = 255;
float m_size = 20;
float m_elevationm_size = 10;

int m_numErosionIterations = 50000;
int m_erosionBrushRadius = 3;

int m_maxLifetime = 30;
float m_inertia = 0.3f;
float m_sedimentCapacityFactor = 3.0f;
float m_minSedimentCapacity = .01f;
float m_depositSpeed = 0.3f;
float m_erodeSpeed = 0.3f;

float m_evaporateSpeed = .01f;
float m_gravity = 4;
float m_startSpeed = 1;
float m_startWater = 1;

float* m_input = NULL;

std::string m_hlslDir;

////////////////////////////////////////////////////////////////////////////////////////////////////////

ID3D11Buffer* CreateAndCopyToDebugBuf( ID3D11Device* pD3DDevice
                                     , ID3D11DeviceContext* deviceContext
                                     , ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* debugbuf = NULL;

    D3D11_BUFFER_DESC BufferDesc;
    ZeroMemory( &BufferDesc, sizeof(D3D11_BUFFER_DESC) );
    pBuffer->GetDesc( &BufferDesc );
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // Set can read from CPU
    BufferDesc.Usage = D3D11_USAGE_STAGING;
    BufferDesc.BindFlags = 0;
    BufferDesc.MiscFlags = 0;
    if( FAILED( pD3DDevice->CreateBuffer( &BufferDesc, NULL, &debugbuf ) ) )
        goto EXIT;

    deviceContext->CopyResource( debugbuf, pBuffer );

EXIT:
   return debugbuf;
}

HRESULT CreateSRVForStructuredBuffer( ID3D11Device* pD3DDevice
                                    , UINT uElementSize
                                    , UINT uCount
                                    , VOID* pInitData
                                    , ID3D11Buffer** ppBuf
                                    , ID3D11ShaderResourceView** ppSRVOut)
{
    HRESULT hr = E_FAIL;
    
    *ppBuf = NULL;
    *ppSRVOut = NULL;

    D3D11_BUFFER_DESC BufferDesc;
    ZeroMemory( &BufferDesc, sizeof(D3D11_BUFFER_DESC) );
    BufferDesc.BindFlags = 
        D3D11_BIND_UNORDERED_ACCESS |
        D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.ByteWidth = uElementSize * uCount;
    BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = uElementSize;

    if ( pInitData )
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        hr = pD3DDevice->CreateBuffer( &BufferDesc, &InitData, ppBuf );
        if( FAILED( hr ) )
            goto EXIT;
    }
    else
    {
        hr = pD3DDevice->CreateBuffer( &BufferDesc, NULL, ppBuf );
        if( FAILED( hr ) )
            goto EXIT;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC) );
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    SRVDesc.BufferEx.FirstElement = 0;
    SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    SRVDesc.BufferEx.NumElements = uCount;

    hr = pD3DDevice->CreateShaderResourceView( *ppBuf, &SRVDesc, ppSRVOut );
    if( FAILED( hr ) )
        goto EXIT;

    hr = S_OK;
EXIT:
   return hr;
}

HRESULT CreateUAVForStructuredBuffer( ID3D11Device* pD3DDevice
                                    , UINT uElementSize
                                    , UINT uCount
                                    , VOID* pInitData
                                    , ID3D11Buffer** ppBuf
                                    , ID3D11UnorderedAccessView** ppUAVOut)
{
    HRESULT hr = E_FAIL;

    *ppBuf = NULL;
    *ppUAVOut = NULL;

    D3D11_BUFFER_DESC BufferDesc;
    ZeroMemory( &BufferDesc, sizeof(D3D11_BUFFER_DESC) );
    BufferDesc.BindFlags = 
        D3D11_BIND_UNORDERED_ACCESS |
        D3D11_BIND_SHADER_RESOURCE;
    BufferDesc.ByteWidth = uElementSize * uCount;
    BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    BufferDesc.StructureByteStride = uElementSize;

    if ( pInitData )
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        hr = pD3DDevice->CreateBuffer( &BufferDesc, &InitData, ppBuf );
        if( FAILED( hr ) )
            goto EXIT;
    }
    else
    {
        hr = pD3DDevice->CreateBuffer( &BufferDesc, NULL, ppBuf );
        if( FAILED( hr ) )
            goto EXIT;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
    ZeroMemory( &UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
    UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    UAVDesc.Buffer.FirstElement = 0;
    UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
    UAVDesc.Buffer.NumElements = uCount;

    hr = pD3DDevice->CreateUnorderedAccessView( *ppBuf, &UAVDesc, ppUAVOut );
    if( FAILED( hr ) ) 
        goto EXIT;
    
    hr = S_OK;
EXIT:
   return hr;
}

HRESULT CreateCBForStructuredBuffer( ID3D11Device* pD3DDevice
                                   , int brushLength
                                   , ID3D11Buffer** ppBuf)
{
    HRESULT hr = E_FAIL;
    *ppBuf = NULL;

    struct ERODE_SETTINGS
    {
        int mapSize;
        int brushLength;
        int borderSize;

        int maxLifetime;
        float inertia;
        float sedimentCapacityFactor;
        float minSedimentCapacity;
        float depositSpeed;
        float erodeSpeed;

        float evaporateSpeed;
        float gravity;
        float startSpeed;
        float startWater;

        float makeup[3] = {0.0f, 0.0f, 0.0f};
    };

    // Supply the vertex shader constant data.
    ERODE_SETTINGS VsConstData;
    VsConstData.mapSize = m_mapSize;
    VsConstData.brushLength = brushLength;
    VsConstData.borderSize = m_erosionBrushRadius;
    VsConstData.maxLifetime = m_maxLifetime;
    VsConstData.inertia = m_inertia;
    VsConstData.sedimentCapacityFactor = m_sedimentCapacityFactor;
    VsConstData.minSedimentCapacity = m_minSedimentCapacity;
    VsConstData.depositSpeed = m_depositSpeed;
    VsConstData.erodeSpeed = m_erodeSpeed;
    VsConstData.evaporateSpeed = m_evaporateSpeed;
    VsConstData.gravity = m_gravity;
    VsConstData.startSpeed = m_startSpeed;
    VsConstData.startWater = m_startWater;

    // Fill in a buffer description.
    D3D11_BUFFER_DESC cbDesc;
    cbDesc.ByteWidth = sizeof( ERODE_SETTINGS );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    // Fill in the subresource data.
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = &VsConstData;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;

    // Create the buffer.
    hr = pD3DDevice->CreateBuffer( &cbDesc, &InitData, ppBuf );
    if( FAILED( hr ) ) 
        goto EXIT;
    
    hr = S_OK;
EXIT:
   return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CompileComputeShader( _In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint,
                              _In_ ID3D11Device* device, _Outptr_ ID3DBlob** blob )
{
    if ( !srcFile || !entryPoint || !device || !blob )
       return E_INVALIDARG;

    *blob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    flags |= D3DCOMPILE_DEBUG;
#endif

    // We generally prefer to use the higher CS shader profile when possible as CS 5.0 is better performance on 11-class hardware
    LPCSTR profile = ( device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ) ? "cs_5_0" : "cs_4_0";

    D3D_SHADER_MACRO defines[] = 
    {
        "EXAMPLE_DEFINE", "1",
        NULL, NULL
    };

    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompileFromFile( srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                     entryPoint, profile,
                                     flags, 0, &shaderBlob, &errorBlob );
    if ( FAILED(hr) )
    {
        if ( errorBlob )
        {
            OutputDebugStringA( (char*)errorBlob->GetBufferPointer() );
            errorBlob->Release();
        }

        if ( shaderBlob )
           shaderBlob->Release();

        return hr;
    }

    *blob = shaderBlob;

    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// https://gist.github.com/siwa32/98704
std::wstring convString(const std::string& input)
{
    size_t i;
    wchar_t* buffer = new wchar_t[input.size() + 1];
    mbstowcs_s(&i, buffer, input.size() + 1, input.c_str(), _TRUNCATE);
    std::wstring result = buffer;
    delete[] buffer;
    return result;
}

// https://gist.github.com/karolisjan/f9b8ac3ae2d41ec0ce70f2feac6bdfaf
std::string GetCurrentDir()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	
	return std::string(buffer).substr(0, pos);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

int erode_simulation()
{
    //
    // Create compute shader
    //

    // Create Device
    D3D_FEATURE_LEVEL lvl[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* deviceContext = nullptr;

    HRESULT hr = D3D11CreateDevice(
                    nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                    createDeviceFlags, lvl, _countof(lvl),
				    D3D11_SDK_VERSION, &device, nullptr, &deviceContext );

    if ( hr == E_INVALIDARG )
    {
        // DirectX 11.0 Runtime doesn't recognize D3D_FEATURE_LEVEL_11_1 as a valid value
        hr = D3D11CreateDevice(
                    nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                    0, &lvl[1], _countof(lvl) - 1,
                    D3D11_SDK_VERSION, &device, nullptr, &deviceContext );
    }

    if ( FAILED(hr) )
    {
        printf("Failed creating Direct3D 11 device %08X\n", hr );
        return -1;
    }

    // Verify compute shader is supported
    if ( device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0 )
    {
        D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts = { 0 } ;
        (void)device->CheckFeatureSupport( D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts) );
        if ( !hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
        {
            device->Release();
            printf( "DirectCompute is not supported by this device\n" );
            return -1;
        }
    }

    // Compile shader
    // std::wstring ---> LPCWSTR: https://stackoverflow.com/questions/22585326/how-to-convert-stdwstring-to-lpctstr-in-c
    // L"" + L"": https://learn.microsoft.com/ja-jp/cpp/text/how-to-convert-between-various-string-types?view=msvc-170
    ID3DBlob *csBlob = nullptr;
    std::wstring hlslDir = convString(m_hlslDir);
    std::wstring hlslName = L"\\Erode.hlsl";
    wprintf(L"%s\n", (hlslDir + hlslName).c_str());
    hr = CompileComputeShader((hlslDir + hlslName).c_str(), "CSMain", device, &csBlob);
    if ( FAILED(hr) )
    {
        device->Release();
        printf("Failed compiling shader %08X\n", hr );
        return -1;
    }

    // Create shader
    ID3D11ComputeShader* computeShader = nullptr;
    hr = device->CreateComputeShader( csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &computeShader );

    csBlob->Release();

    if ( FAILED(hr) )
        device->Release();

    printf("Create compute shader success\n");

    //
    // Erosion settings
    //

    // Create brush
    std::list<int> brushIndexOffsetsList;
    std::list<float> brushWeightsList;

    float weightSum = 0;
    for (int brushY = -m_erosionBrushRadius; brushY <= m_erosionBrushRadius; brushY++) {
        for (int brushX = -m_erosionBrushRadius; brushX <= m_erosionBrushRadius; brushX++) {
            float sqrDst = (float)(brushX * brushX) + (float)(brushY * brushY);
            if (sqrDst < m_erosionBrushRadius * m_erosionBrushRadius) {
                brushIndexOffsetsList.push_front (brushY * m_mapSize + brushX);
                float brushWeight = 1 - std::sqrt (sqrDst) / m_erosionBrushRadius;
                weightSum += brushWeight;
                brushWeightsList.push_front (brushWeight);
            }
        }
    }

    for (int i = 0; i < brushWeightsList.size(); i++) {
        std::list<float>::iterator weight = brushWeightsList.begin();
        std::advance(weight, i);
        *weight /= weightSum;
#if _OUTPUTLOG
        printf("normalized : %f\n", *weight);
#endif
    }

    int k = 0;
    int* brushIndexOffsets = new int[brushIndexOffsetsList.size()];
    for (int i: brushIndexOffsetsList)
        brushIndexOffsets[k++] = i;

    k = 0;
    float* brushWeights = new float[brushWeightsList.size()];
    for (float i: brushWeightsList)
        brushWeights[k++] = i;

#if _OUTPUTLOG
    for( int i = 0; i < brushIndexOffsetsList.size(); i++)
        printf("brush index offset: %d\n", brushIndexOffsets[i]);

    for( int i = 0; i < brushWeightsList.size(); i++)
        printf("brush weight: %f\n", brushWeights[i]);
#endif

    // Create random rains
    std::random_device seed_gen;
    std::mt19937 engine(seed_gen());
    std::uniform_real_distribution<float> dist0((float)m_erosionBrushRadius, (float)m_mapSize + (float)m_erosionBrushRadius);
#if _TEST
    std::uniform_real_distribution<float> dist1(0.0f, 5.0f);
#endif

    int* randomIndices = new int[m_numErosionIterations];
    for (int i = 0; i < m_numErosionIterations; i++) {
        int randomX = (int)dist0(engine);
        int randomY = (int)dist0(engine);
        randomIndices[i] = randomY * m_mapSize + randomX;
    }

#if _TEST
    // Create random value map
    m_input = new float[m_mapSize * m_mapSize];
    for ( int i = 0; i < m_mapSize * m_mapSize; i++ ) 
        m_input[i] = dist1(engine);
#endif

#if _OUTPUTLOG
    printf("befor --------------------\n");

    for( int i = 0; i < m_numErosionIterations; i += m_numErosionIterations / 100 )
        printf( "p[%d]: %f\n", i, m_input[i]);
#endif

    //
    // Execute compute shader
    //

    ID3D11Buffer* pRandomIndices = NULL;
    ID3D11Buffer* pBrushIndexOffsets = NULL;
    ID3D11Buffer* pBrushWeights = NULL;
    ID3D11Buffer* pErodeMap = NULL;
    ID3D11Buffer* pErodeConst = NULL;

    ID3D11ShaderResourceView* pRandomIndicesSRV = NULL;
    ID3D11ShaderResourceView* pBrushIndexOffsetsSRV = NULL;
    ID3D11ShaderResourceView* pBrushWeightsSRV = NULL;
    ID3D11UnorderedAccessView* pErodeMapUAV = NULL;

    hr = CreateSRVForStructuredBuffer( device, sizeof(int)
                                     , m_numErosionIterations
                                     , randomIndices
                                     , &pRandomIndices, &pRandomIndicesSRV );
    if( FAILED( hr ) )
        goto EXIT;

    hr = CreateSRVForStructuredBuffer( device, sizeof(int)
                                     , (UINT)brushWeightsList.size()
                                     , brushIndexOffsets
                                     , &pBrushIndexOffsets, &pBrushIndexOffsetsSRV );
    if( FAILED( hr ) )
        goto EXIT;

    hr = CreateSRVForStructuredBuffer( device, sizeof(float)
                                     , (UINT)brushWeightsList.size()
                                     , brushWeights
                                     , &pBrushWeights, &pBrushWeightsSRV );
    if( FAILED( hr ) )
        goto EXIT;

    hr = CreateUAVForStructuredBuffer( device, sizeof(float)
                                     , m_mapSize * m_mapSize
                                     , m_input
                                     , &pErodeMap, &pErodeMapUAV );
    if( FAILED( hr ) )
        goto EXIT;

    hr = CreateCBForStructuredBuffer( device, (int)brushIndexOffsetsList.size(), &pErodeConst);
    if( FAILED( hr ) )
        goto EXIT;

    // Set compute shader
    deviceContext->CSSetShader( computeShader, NULL, 0 );

    // Set shader resource
    deviceContext->CSSetShaderResources( 0, 1, &pRandomIndicesSRV );  // StructuredBuffer
    deviceContext->CSSetShaderResources( 1, 1, &pBrushIndexOffsetsSRV );  // StructuredBuffer
    deviceContext->CSSetShaderResources( 2, 1, &pBrushWeightsSRV );  // StructuredBuffer
    deviceContext->CSSetConstantBuffers( 0, 1, &pErodeConst);   // Constant buffer
    deviceContext->CSSetUnorderedAccessViews( 0, 1, &pErodeMapUAV, NULL ); // RWStructuredBuffer

    // Dispatch shader
    deviceContext->Dispatch( m_numErosionIterations / 1024, 1, 1 );

    // Unset compute shader
    deviceContext->CSSetShader( NULL, NULL, 0 );

    ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
    deviceContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );

    ID3D11ShaderResourceView* ppSRVNULL[3] = { NULL, NULL, NULL };
    deviceContext->CSSetShaderResources( 0, 3, ppSRVNULL );

    ID3D11Buffer* ppCBNULL[1] = { NULL };
    deviceContext->CSSetConstantBuffers( 0, 1, ppCBNULL );

    // Output compute shader result
    ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( device, deviceContext, pErodeMap );
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    deviceContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource );
    float* p = reinterpret_cast<float*>(MappedResource.pData);

#if _OUTPUTLOG
    printf("after --------------------\n");

    for( int i = 0; i < m_numErosionIterations; i += m_numErosionIterations / 100 )
        if(m_input[i] != p[i])
            printf( "p[%d]: %f\n", i, p[i]);
#endif

    for( int i = 0; i < m_mapSize * m_mapSize; i++)
        m_input[i] = p[i];

    // Clean up
    computeShader->Release();
    device->Release();

    return 0;

EXIT:
    SAFE_RELEASE( pRandomIndicesSRV );
    SAFE_RELEASE( pBrushIndexOffsetsSRV );
    SAFE_RELEASE( pBrushWeightsSRV );
    SAFE_RELEASE( pErodeMapUAV );

    SAFE_RELEASE( pRandomIndices );
    SAFE_RELEASE( pBrushIndexOffsets );
    SAFE_RELEASE( pBrushWeights );
    SAFE_RELEASE( pErodeConst );
    SAFE_RELEASE( pErodeMap );

    SAFE_RELEASE( computeShader );
    SAFE_RELEASE( deviceContext );
    SAFE_RELEASE( device );

    return 0;
}

py::array_t<float> erode(int mapSize
                       , float size
                       , float elevationm_size
                       , int numErosionIterations
                       , int erosionBrushRadius
                       , int maxLifetime
                       , float inertia
                       , float sedimentCapacityFactor
                       , float minSedimentCapacity
                       , float depositSpeed
                       , float erodeSpeed
                       , float evaporateSpeed
                       , float gravity
                       , float startSpeed
                       , float startWater
                       , py::array_t<float> input
                       , std::string hlslDir)
{
    m_mapSize = mapSize;
    m_size = size;
    m_elevationm_size = elevationm_size;
    m_numErosionIterations = numErosionIterations;
    m_erosionBrushRadius = erosionBrushRadius;
    m_maxLifetime = maxLifetime;
    m_inertia = inertia;
    m_sedimentCapacityFactor = sedimentCapacityFactor;
    m_minSedimentCapacity = minSedimentCapacity;
    m_depositSpeed = depositSpeed;
    m_erodeSpeed = erodeSpeed;
    m_evaporateSpeed = evaporateSpeed;
    m_gravity = gravity;
    m_startSpeed = startSpeed;
    m_startWater = startWater;

    m_hlslDir = hlslDir;

    // Create random value map
    m_input = new float[m_mapSize * m_mapSize];

    // copy py::array to float*
    const auto &input_info = input.request();
    const auto &shape = input_info.shape;
    for (int i = 0; i < shape[0]; i++)
        m_input[i] = input.mutable_at(i);

    erode_simulation();

    // copy erosion result to input buffer
    auto output = py::array_t<float>(input_info.size);
    auto output_info = output.request();
    for (int i = 0; i < shape[0]; i++)
        output.mutable_at(i) = m_input[i];

    return output;
}

PYBIND11_MODULE(dx11_erosion, m)
{
    m.doc() = "blender python grid mesh erosion plug-in";
    m.def("erosion", &erode, "");
}