#pragma once
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include <vector>

class MeshCom
{
public:
    struct MeshParam
    {
        float*       posInfo;
        unsigned int posCount;
        uint16_t*    indexInfo;
        unsigned int indexCount;
    };
    struct BufferParam
    {
        Buffer*  m_pBuffer = nullptr;
        uint64_t m_syncToken = 100;
    };
    MeshCom();
    ~MeshCom();

    bool Init(const MeshParam& param);
    void UpdateRes();
    void DrawMesh(Cmd* cmd);
    bool IsLoaded();
    bool Remove();

public:
    BufferParam m_posBuffer;
    BufferParam m_indexBuffer;
    MeshParam   m_param;
};

class Box
{
public:
    Box()
    {
        //m_index = { 0 };
        //m_pos = { 0 };
    };
    ~Box() {};
    struct Param
    {
        float xLength = 1.0f;
        float yLength = 1.0f;
        float zLength = 1.0f;
        float uTile = 1.0f;
        float vTile = 1.0f;
        float wTile = 1.0f;
        float posX = 0.0f;
        float posY = 0.0f;
        float posZ = 0.0f;
    };

public:
    bool GetMeshParam(MeshCom::MeshParam& mesh, Param param)
    {
        m_param = param;
        float pos[] = { m_param.posX,
                        m_param.posY,
                        m_param.posZ,
                        -1,
                        0,
                        0,
                        m_param.wTile,
                        m_param.vTile, // 0
                        m_param.posX,
                        m_param.posY,
                        m_param.posZ,
                        0,
                        -1,
                        0,
                        0,
                        0,
                        m_param.posX,
                        m_param.posY,
                        m_param.posZ,
                        0,
                        0,
                        -1,
                        0,
                        m_param.vTile,
                        m_param.posX + param.xLength,
                        m_param.posY,
                        m_param.posZ,
                        1,
                        0,
                        0,
                        0,
                        m_param.vTile, // 1
                        m_param.posX + param.xLength,
                        m_param.posY,
                        m_param.posZ,
                        0,
                        -1,
                        0,
                        m_param.uTile,
                        0,
                        m_param.posX + param.xLength,
                        m_param.posY,
                        m_param.posZ,
                        0,
                        0,
                        -1,
                        m_param.uTile,
                        m_param.vTile,
                        m_param.posX,
                        m_param.posY,
                        m_param.posZ + param.zLength,
                        -1,
                        0,
                        0,
                        0,
                        m_param.vTile, // 2
                        m_param.posX,
                        m_param.posY,
                        m_param.posZ + param.zLength,
                        0,
                        -1,
                        0,
                        0,
                        m_param.wTile,
                        m_param.posX,
                        m_param.posY,
                        m_param.posZ + param.zLength,
                        0,
                        0,
                        1,
                        m_param.uTile,
                        m_param.vTile,
                        m_param.posX + param.xLength,
                        m_param.posY,
                        m_param.posZ + param.zLength,
                        1,
                        0,
                        0,
                        m_param.wTile,
                        m_param.vTile, // 3
                        m_param.posX + param.xLength,
                        m_param.posY,
                        m_param.posZ + param.zLength,
                        0,
                        -1,
                        0,
                        m_param.uTile,
                        m_param.wTile,
                        m_param.posX + param.xLength,
                        m_param.posY,
                        m_param.posZ + param.zLength,
                        0,
                        0,
                        1,
                        0,
                        m_param.vTile,
                        m_param.posX,
                        m_param.posY + param.yLength,
                        m_param.posZ,
                        -1,
                        0,
                        0,
                        m_param.wTile,
                        0, // 4
                        m_param.posX,
                        m_param.posY + param.yLength,
                        m_param.posZ,
                        0,
                        1,
                        0,
                        0,
                        m_param.wTile,
                        m_param.posX,
                        m_param.posY + param.yLength,
                        m_param.posZ,
                        0,
                        0,
                        -1,
                        0,
                        0,
                        m_param.posX + param.xLength,
                        m_param.posY + param.yLength,
                        m_param.posZ,
                        1,
                        0,
                        0,
                        0,
                        0, // 5
                        m_param.posX + param.xLength,
                        m_param.posY + param.yLength,
                        m_param.posZ,
                        0,
                        1,
                        0,
                        m_param.uTile,
                        m_param.wTile,
                        m_param.posX + param.xLength,
                        m_param.posY + param.yLength,
                        m_param.posZ,
                        0,
                        0,
                        -1,
                        m_param.uTile,
                        0,
                        m_param.posX,
                        m_param.posY + param.yLength,
                        m_param.posZ + param.zLength,
                        -1,
                        0,
                        0,
                        0,
                        0, // 6
                        m_param.posX,
                        m_param.posY + param.yLength,
                        m_param.posZ + param.zLength,
                        0,
                        1,
                        0,
                        0,
                        0,
                        m_param.posX,
                        m_param.posY + param.yLength,
                        m_param.posZ + param.zLength,
                        0,
                        0,
                        1,
                        m_param.uTile,
                        0,
                        m_param.posX + param.xLength,
                        m_param.posY + param.yLength,
                        m_param.posZ + param.zLength,
                        1,
                        0,
                        0,
                        m_param.wTile,
                        0, // 7
                        m_param.posX + param.xLength,
                        m_param.posY + param.yLength,
                        m_param.posZ + param.zLength,
                        0,
                        1,
                        0,
                        m_param.uTile,
                        0,
                        m_param.posX + param.xLength,
                        m_param.posY + param.yLength,
                        m_param.posZ + param.zLength,
                        0,
                        0,
                        1,
                        0,
                        0 };
        memcpy(&m_pos[0], pos, sizeof(pos));

        uint16_t index[] = {
            1 * 3 + 0, 5 * 3 + 0, 7 * 3 + 0, 1 * 3 + 0, 7 * 3 + 0, 3 * 3 + 0, //+x
            2 * 3 + 0, 6 * 3 + 0, 4 * 3 + 0, 2 * 3 + 0, 4 * 3 + 0, 0 * 3 + 0, //-x
            4 * 3 + 1, 6 * 3 + 1, 5 * 3 + 1, 5 * 3 + 1, 6 * 3 + 1, 7 * 3 + 1, //+y
            0 * 3 + 1, 1 * 3 + 1, 2 * 3 + 1, 2 * 3 + 1, 1 * 3 + 1, 3 * 3 + 1, //-y
            2 * 3 + 2, 7 * 3 + 2, 6 * 3 + 2, 2 * 3 + 2, 3 * 3 + 2, 7 * 3 + 2, //+z
            0 * 3 + 2, 4 * 3 + 2, 5 * 3 + 2, 0 * 3 + 2, 5 * 3 + 2, 1 * 3 + 2, //-z
        };

        memcpy(&m_index[0], index, sizeof(index));
        mesh.posInfo = &m_pos[0];
        mesh.posCount = 192;
        mesh.indexInfo = &m_index[0];
        mesh.indexCount = 36;
        return true;
    }

private:
    float    m_pos[192];
    uint16_t m_index[36];
    Param    m_param;
};