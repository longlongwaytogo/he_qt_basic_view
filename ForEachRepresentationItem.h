#pragma once

#include <QtCore>
#include <A3DSDKIncludes.h>
#include <functional>

using EntityArray = QVector<A3DEntity*>;

void forEach_RepresentationItem(A3DAsmModelFile* model_file, std::function<void(EntityArray const&)> const &fcn );