#include <Qt3DRender>
#include "Mesh.h"
#include <Qt3dRender/qbuffer.h>
#include <iostream>
#include <map>
#include <set>
#include <cstring>
#include <tuple>
#include <limits>
using namespace Qt3DCore;
using namespace Qt3DRender;

Qt3DCore::QComponent *createMesh( A3DTessBase *tess_base ) {
    A3DEEntityType tess_type = kA3DTypeUnknown;
    if(A3D_SUCCESS != A3DEntityGetType( tess_base, &tess_type ) ) {
        return nullptr;
    }

    if( tess_type != kA3DTypeTess3D ) {
        return nullptr;
    }

    A3DTessBaseData tbd;
    A3D_INITIALIZE_DATA(A3DTessBaseData, tbd);
    if( A3D_SUCCESS != A3DTessBaseGet( tess_base, &tbd ) ) {
        return nullptr;
    }

    A3DDouble const *coords = tbd.m_pdCoords;
    A3DUns32 const n_coords = tbd.m_uiCoordSize;

    A3DTess3DData t3dd;
    A3D_INITIALIZE_DATA( A3DTess3DData, t3dd );
    if( A3D_SUCCESS != A3DTess3DGet( tess_base, &t3dd ) ) {
        A3DTessBaseGet( nullptr, &tbd );
        return nullptr;
    }

    A3DDouble const *normals = t3dd.m_pdNormals;
    A3DUns32 const n_normals = t3dd.m_uiNormalSize;
   
    A3DDouble const* textureCoords = t3dd.m_pdTextureCoords;
    A3DUns32 const n_textureCoords = t3dd.m_uiTextureCoordSize;

    QVector<quint32> q_indices;
    QByteArray bufferBytes;
    std::vector<float> bufferData;
    // layout: px,py,pz, nx,ny,nz, u,v -> 8 floats
    quint32 stride = sizeof(float) * 6;
    // 解释以下for循环中代码
	// 该循环遍历了所有的面片索引，并根据面片的类型（是否为三角形）来处理顶点数据。对于每个面片，如果它是三角形，则会计算出三角形的数量，
    // 并为每个三角形的三个顶点提取坐标和法线数据。提取的数据被存储在一个字节数组中，同时也会记录每个顶点的索引，以便后续使用。
	// 具体来说，循环中的代码首先检查当前面片是否包含三角形数据（通过检查标志位）。如果是，则根据三角形的数量计算出需要处理的顶点数量，
    // 并调整字节数组的大小以容纳这些顶点的数据。然后，内层循环遍历每个三角形的三个顶点，提取对应的坐标和法线索引，并将它们转换为实际的坐标和法线值，
    // 存储在字节数组中。同时，记录每个顶点的索引，以便后续构建索引缓冲区。
	// 该循环的目的是将面片数据转换为适合渲染的格式，特别是将三角形面片的数据提取出来，并准备好顶点缓冲区和索引缓冲区，以便在渲染时使用。
    int nCount = 0;
    typedef std::pair< std::uint32_t, std::uint32_t> VertexKey;
    typedef std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> VertexKey3;
    std::map<VertexKey, quint32> vertexMap;  // 键到新顶点索引的映射
	std::map<VertexKey3, quint32> vertexMap3;  // 键到新顶点索引的映射
    quint32 nextVertexIndex = 0;           // 下一个可用的新顶点索引
    int maxV = -1;
    int maxN = -1;
	int minV = std::numeric_limits<int>::max();
	int minN = std::numeric_limits<int>::max();
	std::cout << "m_uiTriangulatedIndexSize" << t3dd.m_uiTriangulatedIndexSize << std::endl;
	std::set<int> uniqueVertices; // 用于存储唯一的顶点索引
	std::set<int> uniqueNormals;  // 用于存储唯一的法线索引
	for (int i = 0; i < int(t3dd.m_uiTriangulatedIndexSize); i=i+2) {
		uniqueNormals.insert(t3dd.m_puiTriangulatedIndexes[i ]);
        uniqueVertices.insert(t3dd.m_puiTriangulatedIndexes[i + 1]);
    }
	std::cout << "Unique vertex indices: " << uniqueVertices.size() << std::endl;
	std::cout << "Unique normal indices: " << uniqueNormals.size() << std::endl;

	for (auto tess_face_idx = 0u; tess_face_idx < t3dd.m_uiFaceTessSize; ++tess_face_idx) { // 遍历所有的面片索引
		A3DTessFaceData const& d = t3dd.m_psFaceTessData[tess_face_idx]; // 获取当前面片的数据
        auto sz_tri_idx = 0u;
		auto ti_index = d.m_uiStartTriangulated; // 获取当前面片的三角形索引的起始位置
        if (kA3DTessFaceDataTriangle & d.m_usUsedEntitiesFlags) {
			auto const num_tris = d.m_puiSizesTriangulated[sz_tri_idx++]; //  获取当前面片中三角形的数量
			auto const pt_count = num_tris * 3; // 计算出需要处理的顶点数量（每个三角形有3个顶点）
            auto const old_sz = bufferBytes.size();
			bufferBytes.resize(bufferBytes.size() + stride * pt_count); // 调整字节数组的大小以容纳这些顶点的数据
			auto fptr = reinterpret_cast<float*>(bufferBytes.data() + old_sz); // 获取指向字节数组中当前可用位置的指针，准备存储顶点数据
			for (auto tri = 0u; tri < num_tris; tri++) { // 遍历每个三角形
				for (auto vert = 0u; vert < 3u; vert++) { // 遍历每个三角形的三个顶点
					auto const& normal_index = t3dd.m_puiTriangulatedIndexes[ti_index++]; // 获取当前顶点的法线索引
					auto const& coord_index = t3dd.m_puiTriangulatedIndexes[ti_index++];  // 获取当前顶点的坐标索引
                    quint32 vertexIndex;
                    VertexKey key(coord_index, normal_index);
				   // std::cout << "key: ("  << coord_index << ", " << normal_index << ")" << std::endl;
					if (maxV < int(coord_index))
                        maxV = coord_index;
					if (maxN < int(normal_index))
						maxN = normal_index;
					if (minV > int(coord_index))
						minV = coord_index;
					if (minN > int(normal_index))
                        minN = normal_index;

                    auto it = vertexMap.find(key);
                    if (it == vertexMap.end())
                    {
                        vertexIndex = nextVertexIndex++;
                        vertexMap.insert(std::make_pair(key, vertexIndex));

                        *fptr++ = coords[coord_index];
                        *fptr++ = coords[coord_index + 1];
                        *fptr++ = coords[coord_index + 2];
                        *fptr++ = normals[normal_index];
                        *fptr++ = normals[normal_index + 1];
                        *fptr++ = normals[normal_index + 2];

                        bufferData.push_back(coords[coord_index]);
                        bufferData.push_back(coords[coord_index + 1]);
                        bufferData.push_back(coords[coord_index + 2]);

                        bufferData.push_back(normals[normal_index]);
                        bufferData.push_back(normals[normal_index + 1]);
                        bufferData.push_back(normals[normal_index + 2]);
                    }
                    else
                    {
                        vertexIndex = it->second;
                    }
					//std::cout << "vertexIndex: " << vertexIndex << std::endl;
                    q_indices.push_back(vertexIndex );
                }
            }
        }
		else if (kA3DTessFaceDataTriangleTextured & d.m_usUsedEntitiesFlags && textureCoords != nullptr && n_textureCoords > 0) {
            auto const num_tris = d.m_puiSizesTriangulated[sz_tri_idx++]; //  获取当前面片中三角形的数量
            auto const pt_count = num_tris * 3; // 计算出需要处理的顶点数量（每个三角形有3个顶点）
            auto const old_sz = bufferBytes.size();
			stride = sizeof(float) * (3 + 3 + 2); // 每个顶点包含位置（3个分量）、法线（3个分量）和纹理坐标（2个分量）
            bufferBytes.resize(bufferBytes.size() + stride * pt_count); // 调整字节数组的大小以容纳这些顶点的数据
            auto fptr = reinterpret_cast<float*>(bufferBytes.data() + old_sz); // 获取指向字节数组中当前可用位置的指针，准备存储顶点数据

            // 在处理 textured 三角形前：
            size_t perVertex = 1 + d.m_uiTextureCoordIndexesSize + 1;
            size_t needed = perVertex * pt_count;
            if (ti_index + needed > t3dd.m_uiTriangulatedIndexSize) {
                std::cerr << "Triangulated indexes too small, skip face\n";
                continue;
            }

			int texComponents = 2; // 每个纹理坐标包含 u 和 v 两个分量
            for (auto tri = 0u; tri < num_tris; tri++) { // 遍历每个三角形
                for (auto vert = 0u; vert < 3u; vert++) { // 遍历每个三角形的三个顶点
                    //    Normal1, Texture1-1, Texture1-2, Point1,
                    auto const& normal_index = t3dd.m_puiTriangulatedIndexes[ti_index++]; // 获取当前顶点的法线索引
                
                    std::vector<unsigned int> texIndices;
					texIndices.reserve(d.m_uiTextureCoordIndexesSize);
                    for (auto ti = 0u; ti < d.m_uiTextureCoordIndexesSize; ++ti) {
                        unsigned int texIdx = t3dd.m_puiTriangulatedIndexes[ti_index++]; // 仅一次读取
                        // 校验纹理偏移范围
                        if (texIdx + texComponents - 1 >= n_textureCoords) { std::cout << "tex index err\n"; }
                        texIndices.push_back(texIdx);
                    }
                    auto const& coord_index = t3dd.m_puiTriangulatedIndexes[ti_index++];  // 获取当前顶点的坐标索引
                    // 验证 coord/normal 可安全读取三分量
                    if (coord_index + 2 >= n_coords || normal_index + 2 >= n_normals) {
                        std::cout << "coord or normal index out of range, skip vertex\n";
                        // advance to next vertex
                        // still push a dummy index to keep indices consistent
                    }
                    quint32 vertexIndex;

                    uint32_t tex0 = texIndices.empty() ? std::numeric_limits<uint32_t>::max() : texIndices[0];
                    VertexKey3 key(coord_index, tex0, normal_index);
                  //  std::cout << "key: (" << coord_index << ", " << normal_index << ")" << std::endl;
                    if (maxV < int(coord_index))
                        maxV = coord_index;
                    if (maxN < int(normal_index))
                        maxN = normal_index;
                    if (minV > int(coord_index))
                        minV = coord_index;
                    if (minN > int(normal_index))
                        minN = normal_index;

                    auto it = vertexMap3.find(key);
                    if (it == vertexMap3.end())
                    {
                        vertexIndex = nextVertexIndex++;
                        vertexMap3.insert(std::make_pair(key, vertexIndex));

                        *fptr++ = coords[coord_index];
                        *fptr++ = coords[coord_index + 1];
                        *fptr++ = coords[coord_index + 2];
                        *fptr++ = normals[normal_index];
                        *fptr++ = normals[normal_index + 1];
                        *fptr++ = normals[normal_index + 2];

                        // 读取纹理（如果需要，把第一个 texture index 当作主 UV）
                        float u = 0.0f, v = 0.0f;
                        if (!texIndices.empty()) {
                            unsigned int texBase = texIndices[0]; // 常用第一个为 UV
                            if (texBase + 0 < n_textureCoords) u = static_cast<float>(t3dd.m_pdTextureCoords[texBase + 0]);
                            if (texBase + 1 < n_textureCoords) v = static_cast<float>(t3dd.m_pdTextureCoords[texBase + 1]);
                        }
                        *fptr++ = u;
                        *fptr++ = v;

                        bufferData.push_back(coords[coord_index]);
                        bufferData.push_back(coords[coord_index + 1]);
                        bufferData.push_back(coords[coord_index + 2]);

                        bufferData.push_back(normals[normal_index]);
                        bufferData.push_back(normals[normal_index + 1]);
                        bufferData.push_back(normals[normal_index + 2]);

                        bufferData.push_back(u);
                        bufferData.push_back(v);

                    }
                    else
                    {
                        vertexIndex = it->second;
                    }
                   // std::cout << "vertexIndex: " << vertexIndex << std::endl;
                    q_indices.push_back(vertexIndex);
                }
            }
        }
    }

	std::cout << " index size:" << q_indices.size() << std::endl;
	std::cout << " maxV: " << maxV << ", maxN: " << maxN << std::endl; 
	std::cout << " minV: " << minV << ", minN: " << minN << std::endl;
    A3DTess3DGet( nullptr, &t3dd );
    A3DTessBaseGet( nullptr, &tbd );
    int nNum = bufferData.size();
    int vertexCount = (nNum / 6); // each vertex has 6 floats: px,py,pz,nx,ny,nz u,v 
	if (vertexMap3.size() > 0) {
        vertexCount = (nNum / 8); // each vertex has 8 floats: px,py,pz,nx,ny,nz u,v
    }
    auto buf = new Qt3DRender::QBuffer();
    QByteArray tempArray(nNum * sizeof(float), 0);
    if (nNum > 0) {
        // copy float data into the QByteArray backing the GPU buffer
        memcpy(tempArray.data(), reinterpret_cast<const char*>(bufferData.data()), nNum * sizeof(float));
    }
    buf->setData(tempArray);
    auto geometry = new QGeometry;
    // pass vertexCount (number of vertices), not number of floats
    auto position_attribute = new QAttribute(buf, QAttribute::defaultPositionAttributeName(), QAttribute::Float, 3, vertexCount, 0, stride);
    geometry->addAttribute(position_attribute);

    auto normal_attribute = new QAttribute(buf, QAttribute::defaultNormalAttributeName(), QAttribute::Float, 3, vertexCount, sizeof(float) * 3, stride);
    geometry->addAttribute(normal_attribute);

	if (vertexMap3.size() > 0) {
        auto tex_attribute = new QAttribute(buf, QAttribute::defaultTextureCoordinateAttributeName(), QAttribute::Float, 2, vertexCount, sizeof(float) * 6, stride);
        geometry->addAttribute(tex_attribute);
    }
            
    QByteArray indexBytes;
    QAttribute::VertexBaseType ty;
    if (q_indices.size() < 65536) {
        ty = QAttribute::UnsignedShort;
        indexBytes.resize(q_indices.size() * sizeof(quint16));
        quint16 *usptr = reinterpret_cast<quint16*>(indexBytes.data());
        for (int i = 0; i < int(q_indices.size()); ++i)
            *usptr++ = static_cast<quint16>(q_indices.at(i));
    } else {
        ty = QAttribute::UnsignedInt;
        Q_ASSERT(sizeof(int) == sizeof(quint32));
        indexBytes.resize(q_indices.size() * sizeof(quint32));
        memcpy(indexBytes.data(), reinterpret_cast<const char*>(q_indices.data()), indexBytes.size());
    }

    auto *indexBuffer = new Qt3DRender::QBuffer();
    indexBuffer->setData( indexBytes );
    QAttribute *indexAttribute = new QAttribute(indexBuffer, ty, 1, q_indices.size());
    indexAttribute->setAttributeType(QAttribute::IndexAttribute);
    geometry->addAttribute(indexAttribute);

    auto renderer = new Qt3DRender::QGeometryRenderer();
    renderer->setGeometry( geometry );
    QGeometryRenderer::PrimitiveType pType = renderer->primitiveType();

    return renderer;
}
