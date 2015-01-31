#pragma once

#include "Helpers.h"

template<class T>
class DynamicGPUBuffer
{
public:
    explicit DynamicGPUBuffer(ID3D11Device& Device, ID3D11DeviceContext& DeviceContext, const unsigned int iReserve)
    :m_Device(Device)
    ,m_DeviceContext(DeviceContext)
    ,m_Mapping()
    ,m_iReserved(iReserve)
    ,m_iSize(0)
    ,m_iMapStart(0)
    {
        Grow();
    }

    DynamicGPUBuffer(const DynamicGPUBuffer&) = delete;
    DynamicGPUBuffer& operator=(const DynamicGPUBuffer&) = delete;

    /**
    It's best to call this at the start of each frame
    */
    void Clear()
    {
        m_iSize = 0;
    }

    unsigned int GetSize() const
    {
        return m_iSize;
    }

    unsigned int GetMaxSize() const
    {
        return m_iReserved;
    }

    unsigned int GetNumNewElements() const
    {
        return m_iSize - m_iMapStart;
    }

    unsigned int GetFirstNewElementIndex() const
    {
        return m_iMapStart;
    }

    ID3D11Buffer* Get() const
    {
        assert(m_pBuffer);
        return m_pBuffer.Get();
    }

    ID3D11Buffer* const * GetAddressOf() const
    {
        assert(m_pBuffer);
        return m_pBuffer.GetAddressOf();
    }

    bool IsMapped() const
    {
        return m_Mapping.pData != nullptr;
    }

    void Grow()
    {
        ComPtr<ID3D11Buffer> pOldBuffer(std::move(m_pBuffer));
        if (pOldBuffer)
        {
            assert(m_iSize == m_iReserved);
            m_iReserved *= 2;
        }

        D3D11_BUFFER_DESC Desc;
        Desc.ByteWidth = sizeof(T) * m_iReserved;
        Desc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_WRITE;
        Desc.MiscFlags = 0;

        ThrowIfFail(m_Device.CreateBuffer(&Desc, nullptr, &m_pBuffer), L"Failed to create buffer %s.", typeid(T).name());
        SetResourceName(m_pBuffer, typeid(T).name());

        if (pOldBuffer)
        {
            D3D11_BOX Box;
            Box.left = 0;
            Box.right = m_iSize * sizeof(T);
            Box.top = 0;
            Box.bottom = 1 * sizeof(T);
            Box.front = 0;
            Box.back = 1 * sizeof(T);
            m_DeviceContext.CopySubresourceRegion(m_pBuffer.Get(), 0, 0, 0, 0, pOldBuffer.Get(), 0, &Box);
        }
    }

    void Map()
    {
        MapInternal();
        m_iMapStart = m_iSize; //Track where fresh data begins so users can draw the new data
    }

    void Unmap()
    {
        assert(IsMapped());
        m_DeviceContext.Unmap(m_pBuffer.Get(), 0);
        m_Mapping.pData = nullptr; //For IsMapped()
    }

    T& GetElement()
    {
        assert(IsMapped());
        if (m_iSize == m_iReserved)
        {
            Unmap();
            Grow();
            MapInternal();
        }
        return static_cast<T*>(m_Mapping.pData)[m_iSize++];
    }


protected:
    void MapInternal()
    {
        assert(!IsMapped());

        //MS recommends reusing a buffer with NO_OVERWRITE during a frame, and using DISCARD at the start of a frame.
        //Makes sense, because using only DISCARD would result in the driver creating a ton of buffers.
        m_DeviceContext.Map(m_pBuffer.Get(), 0, m_iSize == 0 ? D3D11_MAP::D3D11_MAP_WRITE_DISCARD : D3D11_MAP::D3D11_MAP_WRITE_NO_OVERWRITE, 0, &m_Mapping);
    }

    ID3D11Device& m_Device;
    ID3D11DeviceContext& m_DeviceContext;

    ComPtr<ID3D11Buffer> m_pBuffer;
    D3D11_MAPPED_SUBRESOURCE m_Mapping;

    unsigned int m_iReserved;
    unsigned int m_iSize;
    unsigned int m_iMapStart; //!< Start index of current Map() call, so users know which data to draw
};