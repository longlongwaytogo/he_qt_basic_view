#include "ForEachRepresentationItem.h"

namespace {
    A3DAsmPartDefinition *getPart( A3DAsmProductOccurrence *po ) {
        if( nullptr == po ) {
            return nullptr;
        }

        A3DAsmProductOccurrenceData pod;
        A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceData, pod);
        if(A3D_SUCCESS != A3DAsmProductOccurrenceGet( po, &pod ) ) {
            return nullptr;
        }
        auto part = pod.m_pPart ? pod.m_pPart : getPart( pod.m_pPrototype );
        A3DAsmProductOccurrenceGet( nullptr, &pod );
        return part;
    }

    void forEach_Impl( EntityArray const &path, std::function<void(EntityArray const&)> const &fcn ) {
        if( path.isEmpty() ) {
            return;
        }
        auto const ntt = path.back();
        if(nullptr == ntt) {
            return;
        }
        qDebug("forEach_Impl: entity ptr=%p", ntt);
        if( A3DEntityGetType == nullptr ) {
            qWarning("forEach_Impl: A3DEntityGetType is null");
            return;
        }
        auto type = kA3DTypeUnknown;
        if(A3D_SUCCESS != A3DEntityGetType( ntt, &type) ) {
            qWarning("forEach_Impl: A3DEntityGetType failed for ptr=%p", ntt);
            return;
        }

        EntityArray children;
        if(kA3DTypeAsmModelFile == type) {
            A3DAsmModelFileData mfd;
            A3D_INITIALIZE_DATA(A3DAsmModelFileData, mfd);
            if(A3D_SUCCESS != A3DAsmModelFileGet( ntt, &mfd ) ) {
                return;
            }
            if( mfd.m_ppPOccurrences && mfd.m_uiPOccurrencesSize > 0 ) {
                for( A3DUns32 i=0; i < mfd.m_uiPOccurrencesSize; ++i ) {
                    if( mfd.m_ppPOccurrences[i] )
                        children.push_back( reinterpret_cast<A3DEntity*>(mfd.m_ppPOccurrences[i]) );
                }
            }
            A3DAsmModelFileGet( nullptr, &mfd );
        } else if( kA3DTypeAsmProductOccurrence == type ) {
            A3DAsmProductOccurrenceData pod;
            A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceData, pod);
            if(A3D_SUCCESS != A3DAsmProductOccurrenceGet( ntt, &pod ) ) {
                return;
            }
            if( pod.m_ppPOccurrences && pod.m_uiPOccurrencesSize > 0 ) {
                for( A3DUns32 i=0; i < pod.m_uiPOccurrencesSize; ++i ) {
                    if( pod.m_ppPOccurrences[i] )
                        children.push_back( reinterpret_cast<A3DEntity*>(pod.m_ppPOccurrences[i]) );
                }
            }
            if( auto part = pod.m_pPart ? pod.m_pPart : getPart( pod.m_pPrototype ) ) {
                children.insert( children.begin(), part );
            }
            A3DAsmProductOccurrenceGet( nullptr, &pod );
        } else if( kA3DTypeAsmPartDefinition == type ) {
            A3DAsmPartDefinitionData pdd;
            A3D_INITIALIZE_DATA(A3DAsmPartDefinitionData, pdd);
            if(A3D_SUCCESS != A3DAsmPartDefinitionGet( ntt, &pdd ) ) {
                return;
            }
            if( pdd.m_ppRepItems && pdd.m_uiRepItemsSize > 0 ) {
                for( A3DUns32 i=0; i < pdd.m_uiRepItemsSize; ++i ) {
                    if( pdd.m_ppRepItems[i] )
                        children.push_back( reinterpret_cast<A3DEntity*>(pdd.m_ppRepItems[i]) );
                }
            }
            A3DAsmPartDefinitionGet( nullptr, &pdd );
        } else {
            if (kA3DTypeRiSet == type) {
                A3DRiSetData risd;
                A3D_INITIALIZE_DATA(A3DRiSetData, risd);
                if (A3D_SUCCESS != A3DRiSetGet(ntt, &risd)) {
                    return;
                }
                if( risd.m_ppRepItems && risd.m_uiRepItemsSize > 0 ) {
                    for( A3DUns32 i=0; i < risd.m_uiRepItemsSize; ++i ) {
                        if( risd.m_ppRepItems[i] )
                            children.push_back( reinterpret_cast<A3DEntity*>(risd.m_ppRepItems[i]) );
                    }
                }
                A3DRiSetGet( nullptr, &risd );
            } else {
                fcn( path );
            }
        }

        for( auto child : children ) {
            auto child_path = path;
            child_path.push_back( child );
            forEach_Impl( child_path, fcn );
        }
    }
}

void forEach_RepresentationItem( A3DAsmModelFile *model_file, std::function<void(EntityArray const&)> const &fcn ) {
    if( nullptr == model_file ) {
        qWarning("forEach_RepresentationItem: model_file is null");
        return;
    }

    // Ensure basic API entry points are available
    if( A3DEntityGetType == nullptr ) {
        qWarning("forEach_RepresentationItem: A3D API not initialized (A3DEntityGetType is null)");
        return;
    }

    forEach_Impl( { model_file }, fcn );
}

 
