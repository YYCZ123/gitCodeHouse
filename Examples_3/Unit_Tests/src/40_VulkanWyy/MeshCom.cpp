#include "MeshCom.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
MeshCom::MeshCom() {}
MeshCom::~MeshCom() {}

bool MeshCom::Init(const MeshParam& param)
{
    m_param = param;
    uint64_t       pointDataSize = m_param.posCount * sizeof(float);
    BufferLoadDesc pointVbDesc = {};
    pointVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    pointVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    pointVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    pointVbDesc.mDesc.mSize = pointDataSize;
    pointVbDesc.mDesc.mAlignment = 8*sizeof(float);
    pointVbDesc.pData = m_param.posInfo;
    pointVbDesc.ppBuffer = &m_posBuffer.m_pBuffer;
    addResource(&pointVbDesc, &m_posBuffer.m_syncToken);
    waitForToken(&m_posBuffer.m_syncToken);

    BufferLoadDesc triIdxDesc = {};
    triIdxDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    triIdxDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    triIdxDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    triIdxDesc.mDesc.mSize = m_param.indexCount * sizeof(uint16_t);
    triIdxDesc.mDesc.mAlignment = sizeof(uint16_t);
    triIdxDesc.pData = m_param.indexInfo;
    triIdxDesc.ppBuffer = &m_indexBuffer.m_pBuffer;
    addResource(&triIdxDesc, &m_indexBuffer.m_syncToken);
    waitForToken(&m_indexBuffer.m_syncToken);

    return true;
}

bool MeshCom::IsLoaded()
{
    if (isTokenCompleted(&m_posBuffer.m_syncToken) && isTokenCompleted(&m_indexBuffer.m_syncToken))
        return true;
    return false;
}

bool MeshCom::Remove() 
{ 
    removeResource(m_posBuffer.m_pBuffer);
    removeResource(m_indexBuffer.m_pBuffer);
    return true; 
}

void MeshCom::UpdateRes()
{
    BufferUpdateDesc upTriVertexBuf = { m_posBuffer.m_pBuffer };
    beginUpdateResource(&upTriVertexBuf);
    memcpy(upTriVertexBuf.pMappedData, m_param.posInfo, m_param.posCount * sizeof(float));
    endUpdateResource(&upTriVertexBuf);

    BufferUpdateDesc upTriIdxBuf = { m_indexBuffer.m_pBuffer };
    beginUpdateResource(&upTriIdxBuf);
    memcpy(upTriIdxBuf.pMappedData, m_param.indexInfo, m_param.indexCount * sizeof(uint16_t));
    endUpdateResource(&upTriIdxBuf);
}
void MeshCom::DrawMesh(Cmd* cmd)
{
    const uint32_t triangleVbStride = sizeof(float) * 8;
    cmdBindVertexBuffer(cmd, 1, &m_posBuffer.m_pBuffer, &triangleVbStride, nullptr);
    cmdBindIndexBuffer(cmd, m_indexBuffer.m_pBuffer, INDEX_TYPE_UINT16, 0);
    cmdDrawIndexedInstanced(cmd, m_param.indexCount, 0, 1, 0, 0);
}