#include <Qt3DRender>
#include "Mesh.h"
#include <Qt3dRender/qbuffer.h>

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
   
    QVector<quint32> q_indices;
    QByteArray bufferBytes;
    quint32 const stride = sizeof(float) * 6;
    // 解释以下for循环中代码
	// 该循环遍历了所有的面片索引，并根据面片的类型（是否为三角形）来处理顶点数据。对于每个面片，如果它是三角形，则会计算出三角形的数量，
    // 并为每个三角形的三个顶点提取坐标和法线数据。提取的数据被存储在一个字节数组中，同时也会记录每个顶点的索引，以便后续使用。
	// 具体来说，循环中的代码首先检查当前面片是否包含三角形数据（通过检查标志位）。如果是，则根据三角形的数量计算出需要处理的顶点数量，
    // 并调整字节数组的大小以容纳这些顶点的数据。然后，内层循环遍历每个三角形的三个顶点，提取对应的坐标和法线索引，并将它们转换为实际的坐标和法线值，
    // 存储在字节数组中。同时，记录每个顶点的索引，以便后续构建索引缓冲区。
	// 该循环的目的是将面片数据转换为适合渲染的格式，特别是将三角形面片的数据提取出来，并准备好顶点缓冲区和索引缓冲区，以便在渲染时使用。
    
	for (auto tess_face_idx = 0u; tess_face_idx < t3dd.m_uiFaceTessSize; ++tess_face_idx) { // 遍历所有的面片索引
		A3DTessFaceData const& d = t3dd.m_psFaceTessData[tess_face_idx]; // 获取当前面片的数据
        auto sz_tri_idx = 0u;
		auto ti_index = d.m_uiStartTriangulated; // 获取当前面片的三角形索引的起始位置
        if(kA3DTessFaceDataTriangle & d.m_usUsedEntitiesFlags) {
			auto const num_tris = d.m_puiSizesTriangulated[sz_tri_idx++]; //  获取当前面片中三角形的数量
			auto const pt_count = num_tris * 3; // 计算出需要处理的顶点数量（每个三角形有3个顶点）
            auto const old_sz = bufferBytes.size();
			bufferBytes.resize(bufferBytes.size() + stride * pt_count); // 调整字节数组的大小以容纳这些顶点的数据
			auto fptr = reinterpret_cast<float*>(bufferBytes.data() + old_sz); // 获取指向字节数组中当前可用位置的指针，准备存储顶点数据
			for (auto tri = 0u; tri < num_tris; tri++) { // 遍历每个三角形
				for (auto vert = 0u; vert < 3u; vert++) { // 遍历每个三角形的三个顶点
					auto const& normal_index = t3dd.m_puiTriangulatedIndexes[ti_index++]; // 获取当前顶点的法线索引
					auto const& coord_index = t3dd.m_puiTriangulatedIndexes[ti_index++];  // 获取当前顶点的坐标索引
                    // 
                    *fptr++ = coords[coord_index];
                    *fptr++ = coords[coord_index+1];
                    *fptr++ = coords[coord_index+2];
                    *fptr++ = normals[normal_index];
                    *fptr++ = normals[normal_index+1];
                    *fptr++ = normals[normal_index+2];
                    q_indices.push_back( q_indices.size() );
                }
            }
        }
    }

    A3DTess3DGet( nullptr, &t3dd );
    A3DTessBaseGet( nullptr, &tbd );

    auto buf = new Qt3DRender::QBuffer();
    buf->setData( bufferBytes );
    auto geometry = new QGeometry;
    auto position_attribute = new QAttribute( buf, QAttribute::defaultPositionAttributeName(), QAttribute::Float, 3, q_indices.size(), 0, stride );
    geometry->addAttribute( position_attribute );

    auto normal_attribute = new QAttribute( buf, QAttribute::defaultNormalAttributeName(), QAttribute::Float, 3, q_indices.size(), sizeof(float) * 3, stride );
    geometry->addAttribute( normal_attribute );

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

    return renderer;
}
