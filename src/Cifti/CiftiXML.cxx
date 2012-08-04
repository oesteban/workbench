/*LICENSE_START*/
/*
 *  Copyright 1995-2011 Washington University School of Medicine
 *
 *  http://brainmap.wustl.edu
 *
 *  This file is part of CARET.
 *
 *  CARET is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  CARET is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CARET; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*LICENSE_END*/

#include <cmath>
#include "CiftiXML.h"
#include "CiftiFileException.h"
#include "FloatMatrix.h"
#include "CaretAssert.h"

using namespace caret;
using namespace std;

CiftiXML::CiftiXML()
{
    m_rowMapIndex = -1;
    m_colMapIndex = -1;
}

AString CiftiXML::getVersion() const
{
    return m_root.m_version;
}

int64_t CiftiXML::getSurfaceIndex(const int64_t& node, const CiftiBrainModelElement* myElement) const
{
    if (myElement == NULL || myElement->m_modelType != CIFTI_MODEL_TYPE_SURFACE) return -1;
    if (node < 0 || node > (int64_t)(myElement->m_surfaceNumberOfNodes)) return -1;
    CaretAssertVectorIndex(myElement->m_nodeToIndexLookup, node);
    return myElement->m_nodeToIndexLookup[node];
}

int64_t CiftiXML::getColumnIndexForNode(const int64_t& node, const StructureEnum::Enum& structure) const
{
    return getSurfaceIndex(node, findSurfaceModel(m_rowMapIndex, structure));//a column index is an index to get an entire column, so index ALONG a row
}

int64_t CiftiXML::getRowIndexForNode(const int64_t& node, const StructureEnum::Enum& structure) const
{
    return getSurfaceIndex(node, findSurfaceModel(m_colMapIndex, structure));
}

int64_t CiftiXML::getVolumeIndex(const int64_t* ijk, const int& myMapIndex) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0) return -1;
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS)
    {
        return -1;
    }
    const CiftiVolumeElement& myVol = m_root.m_matrices[0].m_volume[0];
    if (ijk[0] < 0 || ijk[0] >= (int64_t)myVol.m_volumeDimensions[0]) return -1;//some shortcuts to not search all the voxels on invalid coords
    if (ijk[1] < 0 || ijk[1] >= (int64_t)myVol.m_volumeDimensions[1]) return -1;
    if (ijk[2] < 0 || ijk[2] >= (int64_t)myVol.m_volumeDimensions[2]) return -1;
    for (int64_t i = 0; i < (int64_t)myMap->m_brainModels.size(); ++i)
    {
        if (myMap->m_brainModels[i].m_modelType == CIFTI_MODEL_TYPE_VOXELS)
        {
            const vector<voxelIndexType>& myVoxels = myMap->m_brainModels[i].m_voxelIndicesIJK;
            int64_t voxelArraySize = (int64_t)myVoxels.size();
            for (int64_t j = 0; j < voxelArraySize; j += 3)
            {
                if ((ijk[0] == (int64_t)myVoxels[j]) && (ijk[1] == (int64_t)myVoxels[j + 1]) && (ijk[2] == (int64_t)myVoxels[j + 2]))
                {
                    return myMap->m_brainModels[i].m_indexOffset + j / 3;
                }
            }
        }
    }
    return -1;
}

int64_t CiftiXML::getColumnIndexForVoxel(const int64_t* ijk) const
{
    return getVolumeIndex(ijk, m_rowMapIndex);
}

int64_t CiftiXML::getRowIndexForVoxel(const int64_t* ijk) const
{
    return getVolumeIndex(ijk, m_colMapIndex);
}

bool CiftiXML::getSurfaceMapping(vector<CiftiSurfaceMap>& mappingOut, const CiftiBrainModelElement* myModel) const
{
    if (myModel == NULL || myModel->m_modelType != CIFTI_MODEL_TYPE_SURFACE)
    {
        mappingOut.clear();
        return false;
    }
    int64_t mappingSize = (int64_t)myModel->m_indexCount;
    mappingOut.resize(mappingSize);
    if (myModel->m_nodeIndices.size() == 0)
    {
        for (int i = 0; i < mappingSize; ++i)
        {
            mappingOut[i].m_ciftiIndex = myModel->m_indexOffset + i;
            mappingOut[i].m_surfaceNode = i;
        }
    } else {
        for (int i = 0; i < mappingSize; ++i)
        {
            mappingOut[i].m_ciftiIndex = myModel->m_indexOffset + i;
            mappingOut[i].m_surfaceNode = myModel->m_nodeIndices[i];
        }
    }
    return true;
}

bool CiftiXML::getSurfaceMapForColumns(vector<CiftiSurfaceMap>& mappingOut, const StructureEnum::Enum& structure) const
{
    return getSurfaceMapping(mappingOut, findSurfaceModel(m_colMapIndex, structure));
}

bool CiftiXML::getSurfaceMapForRows(vector<CiftiSurfaceMap>& mappingOut, const StructureEnum::Enum& structure) const
{
    return getSurfaceMapping(mappingOut, findSurfaceModel(m_rowMapIndex, structure));
}

bool CiftiXML::getVolumeMapping(vector<CiftiVolumeMap>& mappingOut, const int& myMapIndex) const
{
    mappingOut.clear();
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS)
    {
        return false;
    }
    int64_t myIndex = 0;
    bool first = true;
    for (int64_t i = 0; i < (int64_t)myMap->m_brainModels.size(); ++i)
    {
        if (myMap->m_brainModels[i].m_modelType == CIFTI_MODEL_TYPE_VOXELS)
        {
            const vector<voxelIndexType>& myVoxels = myMap->m_brainModels[i].m_voxelIndicesIJK;
            int64_t voxelArraySize = (int64_t)myVoxels.size();
            int64_t modelOffset = myMap->m_brainModels[i].m_indexOffset;
            int64_t j1 = 0;
            if (first)
            {
                mappingOut.reserve(voxelArraySize / 3);//skip the tiny vector reallocs
                first = false;
            }
            for (int64_t j = 0; j < voxelArraySize; j += 3)
            {
                mappingOut.push_back(CiftiVolumeMap());//default constructor should be NOOP and get removed by compiler
                mappingOut[myIndex].m_ciftiIndex = modelOffset + j1;
                mappingOut[myIndex].m_ijk[0] = myVoxels[j];
                mappingOut[myIndex].m_ijk[1] = myVoxels[j + 1];
                mappingOut[myIndex].m_ijk[2] = myVoxels[j + 2];
                ++j1;
                ++myIndex;
            }
        }
    }
    return true;
}

bool CiftiXML::getVolumeMapForColumns(vector<CiftiVolumeMap>& mappingOut) const
{
    return getVolumeMapping(mappingOut, m_colMapIndex);
}

bool CiftiXML::getVolumeMapForRows(vector<CiftiVolumeMap>& mappingOut) const
{
    return getVolumeMapping(mappingOut, m_rowMapIndex);
}

bool CiftiXML::getVolumeStructureMapping(vector<CiftiVolumeMap>& mappingOut, const StructureEnum::Enum& structure, const int& myMapIndex) const
{
    mappingOut.clear();
    const CiftiBrainModelElement* myModel = findVolumeModel(myMapIndex, structure);
    if (myModel == NULL)
    {
        return false;
    }
    int64_t size = (int64_t)myModel->m_voxelIndicesIJK.size();
    CaretAssert(size % 3 == 0);
    mappingOut.resize(size / 3);
    int64_t index = 0;
    for (int64_t i = 0; i < size; i += 3)
    {
        mappingOut[index].m_ciftiIndex = myModel->m_indexOffset + index;
        mappingOut[index].m_ijk[0] = myModel->m_voxelIndicesIJK[i];
        mappingOut[index].m_ijk[1] = myModel->m_voxelIndicesIJK[i + 1];
        mappingOut[index].m_ijk[2] = myModel->m_voxelIndicesIJK[i + 2];
        ++index;
    }
    return true;
}

bool CiftiXML::getVolumeStructureMapForColumns(vector<CiftiVolumeMap>& mappingOut, const StructureEnum::Enum& structure) const
{
    return getVolumeStructureMapping(mappingOut, structure, m_colMapIndex);
}

bool CiftiXML::getVolumeStructureMapForRows(vector<CiftiVolumeMap>& mappingOut, const StructureEnum::Enum& structure) const
{
    return getVolumeStructureMapping(mappingOut, structure, m_rowMapIndex);
}

bool CiftiXML::getVolumeModelMappings(vector<CiftiVolumeStructureMap>& mappingsOut, const int& myMapIndex) const
{
    mappingsOut.clear();
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS)
    {
        return false;
    }
    int numModels = (int)myMap->m_brainModels.size();
    mappingsOut.reserve(numModels);
    for (int i = 0; i < numModels; ++i)
    {
        if (myMap->m_brainModels[i].m_modelType == CIFTI_MODEL_TYPE_VOXELS)
        {
            mappingsOut.push_back(CiftiVolumeStructureMap());
            int whichMap = (int)mappingsOut.size() - 1;
            mappingsOut[whichMap].m_structure = myMap->m_brainModels[i].m_brainStructure;
            int numIndices = (int)myMap->m_brainModels[i].m_indexCount;
            mappingsOut[whichMap].m_map.resize(numIndices);
            for (int index = 0; index < numIndices; ++index)
            {
                mappingsOut[whichMap].m_map[index].m_ciftiIndex = myMap->m_brainModels[i].m_indexOffset + index;
                int64_t i3 = index * 3;
                mappingsOut[whichMap].m_map[index].m_ijk[0] = myMap->m_brainModels[i].m_voxelIndicesIJK[i3];
                mappingsOut[whichMap].m_map[index].m_ijk[1] = myMap->m_brainModels[i].m_voxelIndicesIJK[i3 + 1];
                mappingsOut[whichMap].m_map[index].m_ijk[2] = myMap->m_brainModels[i].m_voxelIndicesIJK[i3 + 2];
            }
        }
    }
    return true;
}

bool CiftiXML::getVolumeModelMapsForColumns(vector<CiftiVolumeStructureMap>& mappingsOut) const
{
    return getVolumeModelMappings(mappingsOut, m_colMapIndex);
}

bool CiftiXML::getVolumeModelMapsForRows(vector<CiftiVolumeStructureMap>& mappingsOut) const
{
    return getVolumeModelMappings(mappingsOut, m_rowMapIndex);
}

bool CiftiXML::getStructureLists(vector<StructureEnum::Enum>& surfaceList, vector<StructureEnum::Enum>& volumeList, const int& myMapIndex) const
{
    surfaceList.clear();
    volumeList.clear();
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS)
    {
        return false;
    }
    int numModels = (int)myMap->m_brainModels.size();
    for (int i = 0; i < numModels; ++i)
    {
        switch (myMap->m_brainModels[i].m_modelType)
        {
            case CIFTI_MODEL_TYPE_SURFACE:
                surfaceList.push_back(myMap->m_brainModels[i].m_brainStructure);
                break;
            case CIFTI_MODEL_TYPE_VOXELS:
                volumeList.push_back(myMap->m_brainModels[i].m_brainStructure);
                break;
            default:
                break;
        }
    }
    return true;
}

bool CiftiXML::getStructureListsForColumns(vector<StructureEnum::Enum>& surfaceList, vector<StructureEnum::Enum>& volumeList) const
{
    return getStructureLists(surfaceList, volumeList, m_colMapIndex);
}

bool CiftiXML::getStructureListsForRows(vector<StructureEnum::Enum>& surfaceList, vector<StructureEnum::Enum>& volumeList) const
{
    return getStructureLists(surfaceList, volumeList, m_rowMapIndex);
}

void CiftiXML::rootChanged()
{//and here is where the real work is done
    m_colMapIndex = -1;//first, invalidate everything
    m_rowMapIndex = -1;
    if (m_root.m_matrices.size() == 0)
    {
        return;//it shouldn't crash if it has no matrix, so return instead of throw
    }
    CiftiMatrixElement& myMatrix = m_root.m_matrices[0];//assume only one matrix
    int numMaps = (int)myMatrix.m_matrixIndicesMap.size();
    for (int i = 0; i < numMaps; ++i)
    {
        CiftiMatrixIndicesMapElement& myMap = myMatrix.m_matrixIndicesMap[i];
        int numDimensions = (int)myMap.m_appliesToMatrixDimension.size();
        for (int j = 0; j < numDimensions; ++j)
        {
            if (myMap.m_appliesToMatrixDimension[j] == 1)//QUIRK: "applies to dimension" means the opposite of what it sounds like - "applies to rows" means a SINGLE row matches a SINGLE element in the "applies to rows" map
            {
                if (m_rowMapIndex != -1)//I am deliberately using the opposite convention, that "applies to rows" means "applies to the full length of a row", because otherwise I will go insane
                {
                    throw CiftiFileException("Multiple mappings on the same dimension not supported");
                }
                m_rowMapIndex = i;
                myMap.setupLookup();
            }
            if (myMap.m_appliesToMatrixDimension[j] == 0)
            {
                if (m_colMapIndex != -1)
                {
                    throw CiftiFileException("Multiple mappings on the same dimension not supported");
                }
                m_colMapIndex = i;
                myMap.setupLookup();
            }
        }
    }
}

int64_t CiftiXML::getColumnSurfaceNumberOfNodes(const StructureEnum::Enum& structure) const
{
    const CiftiBrainModelElement* myModel = findSurfaceModel(m_colMapIndex, structure);
    if (myModel == NULL) return -1;//should this return 0? surfaces shouldn't have 0 nodes, so it seems like it would also make sense as an error value
    return myModel->m_surfaceNumberOfNodes;
}

int64_t CiftiXML::getRowSurfaceNumberOfNodes(const StructureEnum::Enum& structure) const
{
    const CiftiBrainModelElement* myModel = findSurfaceModel(m_rowMapIndex, structure);
    if (myModel == NULL) return -1;
    return myModel->m_surfaceNumberOfNodes;
}

int64_t CiftiXML::getVolumeIndex(const float* xyz, const int& myMapIndex) const
{
    if (m_root.m_matrices.size() == 0)
    {
        return -1;
    }
    if (m_root.m_matrices[0].m_volume.size() == 0)
    {
        return -1;
    }
    const CiftiVolumeElement& myVol = m_root.m_matrices[0].m_volume[0];
    if (myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ.size() == 0)
    {
        return -1;
    }
    const TransformationMatrixVoxelIndicesIJKtoXYZElement& myTrans = myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ[0];//oh the humanity
    FloatMatrix myMatrix = FloatMatrix::zeros(4, 4);
    for (int i = 0; i < 3; ++i)//NEVER trust the fourth row of input, NEVER!
    {
        for (int j = 0; j < 4; ++j)
        {
            myMatrix[i][j] = myTrans.m_transform[i * 4 + j];
        }
    }
    switch (myTrans.m_unitsXYZ)
    {
        case NIFTI_UNITS_MM:
            break;
        case NIFTI_UNITS_METER:
            myMatrix *= 1000.0f;
            break;
        case NIFTI_UNITS_MICRON:
            myMatrix *= 0.001f;
            break;
        default:
            return -1;
    };
    myMatrix[3][3] = 1.0f;//i COULD do this by making a fake volume file, but that seems kinda hacky
    FloatMatrix toIndexSpace = myMatrix.inverse();//invert to convert the other direction
    FloatMatrix myCoord = FloatMatrix::zeros(4, 1);//column vector
    myCoord[0][0] = xyz[0];
    myCoord[1][0] = xyz[1];
    myCoord[2][0] = xyz[2];
    myCoord[3][0] = 1.0f;
    FloatMatrix myIndices = toIndexSpace * myCoord;//matrix multiply
    int64_t ijk[3];
    ijk[0] = (int64_t)floor(myIndices[0][0] + 0.5f);
    ijk[1] = (int64_t)floor(myIndices[1][0] + 0.5f);
    ijk[2] = (int64_t)floor(myIndices[2][0] + 0.5f);
    return getVolumeIndex(ijk, myMapIndex);
}

int64_t CiftiXML::getColumnIndexForVoxelCoordinate(const float* xyz) const
{
    return getVolumeIndex(xyz, m_rowMapIndex);
}

int64_t CiftiXML::getRowIndexForVoxelCoordinate(const float* xyz) const
{
    return getVolumeIndex(xyz, m_colMapIndex);
}

int64_t CiftiXML::getTimestepIndex(const float& seconds, const int& myMapIndex) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    float myStep;
    if (!getTimestep(myStep, myMapIndex))
    {
        return -1;
    }
    float rawIndex = seconds / myStep;
    int64_t ret = (int64_t)floor(rawIndex + 0.5f);
    if (ret < 0 || ret >= myMap->m_numTimeSteps) return -1;//NOTE: should this have a different error value if it is after the end of the timeseries
    return ret;
}

int64_t CiftiXML::getColumnIndexForTimepoint(const float& seconds) const
{
    return getTimestepIndex(seconds, m_rowMapIndex);
}

int64_t CiftiXML::getRowIndexForTimepoint(const float& seconds) const
{
    return getTimestepIndex(seconds, m_colMapIndex);
}

bool CiftiXML::getTimestep(float& seconds, const int& myMapIndex) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_TIME_POINTS)
    {
        return false;
    }
    switch (myMap->m_timeStepUnits)
    {
        case NIFTI_UNITS_SEC:
            seconds = myMap->m_timeStep;
            break;
        case NIFTI_UNITS_MSEC:
            seconds = myMap->m_timeStep * 0.001f;
            break;
        case NIFTI_UNITS_USEC:
            seconds = myMap->m_timeStep * 0.000001f;
            break;
        default:
            return false;
    };
    return true;
}

bool CiftiXML::getColumnTimestep(float& seconds) const
{
    return getTimestep(seconds, m_colMapIndex);
}

bool CiftiXML::getRowTimestep(float& seconds) const
{
    return getTimestep(seconds, m_rowMapIndex);
}

bool CiftiXML::getColumnNumberOfTimepoints(int& numTimepoints) const
{
    if (m_colMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_TIME_POINTS)
    {
        return false;
    }
    numTimepoints = myMap->m_numTimeSteps;
    return true;
}

bool CiftiXML::getRowNumberOfTimepoints(int& numTimepoints) const
{
    if (m_rowMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_TIME_POINTS)
    {
        return false;
    }
    numTimepoints = myMap->m_numTimeSteps;
    return true;
}

bool CiftiXML::getParcelsForColumns(vector<CiftiParcelElement>& parcelsOut) const
{
    if (m_colMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        parcelsOut.clear();
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS)
    {
        parcelsOut.clear();
        return false;
    }
    parcelsOut = myMap->m_parcels;
    return true;
}

bool CiftiXML::getParcelsForRows(vector<CiftiParcelElement>& parcelsOut) const
{
    if (m_rowMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        parcelsOut.clear();
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS)
    {
        parcelsOut.clear();
        return false;
    }
    parcelsOut = myMap->m_parcels;
    return true;
}

int64_t CiftiXML::getParcelForNode(const int64_t& node, const StructureEnum::Enum& structure, const int& myMapIndex) const
{
    if (node < 0 || myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return -1;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS)
    {
        return -1;
    }
    for (int i = 0; i < (int)myMap.m_parcelSurfaces.size(); ++i)
    {
        if (myMap.m_parcelSurfaces[i].m_structure == structure)
        {
            if (node < myMap.m_parcelSurfaces[i].m_numNodes)
            {
                return myMap.m_parcelSurfaces[i].m_lookup[node];
            } else {
                return -1;
            }
        }
    }
    return -1;
}

int64_t CiftiXML::getColumnParcelForNode(const int64_t& node, const StructureEnum::Enum& structure) const
{
    return getParcelForNode(node, structure, m_colMapIndex);
}

int64_t CiftiXML::getRowParcelForNode(const int64_t& node, const caret::StructureEnum::Enum& structure) const
{
    return getParcelForNode(node, structure, m_rowMapIndex);
}

int64_t CiftiXML::getParcelForVoxel(const int64_t* ijk, const int& myMapIndex) const
{
    if (ijk[0] < 0 || ijk[1] < 0 || ijk[2] < 0 || myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return -1;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS)
    {
        return -1;
    }
    for (int i = 0; i < (int)myMap.m_parcels.size(); ++i)
    {
        for (int j = 0; j < (int)myMap.m_parcels[i].m_voxelIndicesIJK.size(); j += 3)
        {
            if (ijk[0] == myMap.m_parcels[i].m_voxelIndicesIJK[j] &&
                ijk[1] == myMap.m_parcels[i].m_voxelIndicesIJK[j + 1] &&
                ijk[2] == myMap.m_parcels[i].m_voxelIndicesIJK[j + 2])
            {
                return i;
            }
        }
    }
    return -1;
}

int64_t CiftiXML::getColumnParcelForVoxel(const int64_t* ijk) const
{
    return getParcelForVoxel(ijk, m_colMapIndex);
}

int64_t CiftiXML::getRowParcelForVoxel(const int64_t* ijk) const
{
    return getParcelForVoxel(ijk, m_rowMapIndex);
}

bool CiftiXML::setColumnNumberOfTimepoints(const int& numTimepoints)
{
    if (m_colMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
    CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_TIME_POINTS)
    {
        return false;
    }
    myMap->m_numTimeSteps = numTimepoints;
    return true;
}

bool CiftiXML::setRowNumberOfTimepoints(const int& numTimepoints)
{
    if (m_rowMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
    CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_TIME_POINTS)
    {
        return false;
    }
    myMap->m_numTimeSteps = numTimepoints;
    return true;
}

bool CiftiXML::setTimestep(const float& seconds, const int& myMapIndex)
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_TIME_POINTS)
    {
        return false;
    }
    myMap->m_timeStepUnits = NIFTI_UNITS_SEC;
    myMap->m_timeStep = seconds;
    return true;
}

bool CiftiXML::setColumnTimestep(const float& seconds)
{
    return setTimestep(seconds, m_colMapIndex);
}

bool CiftiXML::setRowTimestep(const float& seconds)
{
    return setTimestep(seconds, m_rowMapIndex);
}

bool CiftiXML::getVolumeAttributesForPlumb(VolumeFile::OrientTypes orientOut[3], int64_t dimensionsOut[3], float originOut[3], float spacingOut[3]) const
{
    if (m_root.m_matrices.size() == 0)
    {
        return false;
    }
    if (m_root.m_matrices[0].m_volume.size() == 0)
    {
        return false;
    }
    const CiftiVolumeElement& myVol = m_root.m_matrices[0].m_volume[0];
    if (myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ.size() == 0)
    {
        return false;
    }
    const TransformationMatrixVoxelIndicesIJKtoXYZElement& myTrans = myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ[0];//oh the humanity
    FloatMatrix myMatrix = FloatMatrix::zeros(3, 4);//no fourth row
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            myMatrix[i][j] = myTrans.m_transform[i * 4 + j];
        }
    }
    switch (myTrans.m_unitsXYZ)
    {
        case NIFTI_UNITS_MM:
            break;
        case NIFTI_UNITS_METER:
            myMatrix *= 1000.0f;
            break;
        case NIFTI_UNITS_MICRON:
            myMatrix *= 0.001f;
            break;
        default:
            return false;
    };
    dimensionsOut[0] = myVol.m_volumeDimensions[0];
    dimensionsOut[1] = myVol.m_volumeDimensions[1];
    dimensionsOut[2] = myVol.m_volumeDimensions[2];
    char axisUsed = 0;
    char indexUsed = 0;
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            if (myMatrix[i][j] != 0.0f)
            {
                if (axisUsed & (1<<i))
                {
                    return false;
                }
                if (indexUsed & (1<<j))
                {
                    return false;
                }
                axisUsed &= (1<<i);
                indexUsed &= (1<<j);
                spacingOut[j] = myMatrix[i][j];
                originOut[j] = myMatrix[i][3];
                bool negative;
                if (myMatrix[i][j] > 0.0f)
                {
                    negative = true;
                } else {
                    negative = false;
                }
                switch (i)
                {
                case 0:
                    //left/right
                    orientOut[j] = (negative ? VolumeFile::RIGHT_TO_LEFT : VolumeFile::LEFT_TO_RIGHT);
                    break;
                case 1:
                    //forward/back
                    orientOut[j] = (negative ? VolumeFile::ANTERIOR_TO_POSTERIOR : VolumeFile::POSTERIOR_TO_ANTERIOR);
                    break;
                case 2:
                    //up/down
                    orientOut[j] = (negative ? VolumeFile::SUPERIOR_TO_INFERIOR : VolumeFile::INFERIOR_TO_SUPERIOR);
                    break;
                default:
                    //will never get called
                    break;
                };
            }
        }
    }
    return true;
}

bool CiftiXML::getVolumeDimsAndSForm(int64_t dimsOut[3], vector<vector<float> >& sformOut) const
{
    if (m_root.m_matrices.size() == 0)
    {
        return false;
    }
    if (m_root.m_matrices[0].m_volume.size() == 0)
    {
        return false;
    }
    const CiftiVolumeElement& myVol = m_root.m_matrices[0].m_volume[0];
    if (myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ.size() == 0)
    {
        return false;
    }
    const TransformationMatrixVoxelIndicesIJKtoXYZElement& myTrans = myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ[0];//oh the humanity
    FloatMatrix myMatrix = FloatMatrix::zeros(3, 4);//no fourth row
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            myMatrix[i][j] = myTrans.m_transform[i * 4 + j];
        }
    }
    switch (myTrans.m_unitsXYZ)
    {
        case NIFTI_UNITS_MM:
            break;
        case NIFTI_UNITS_METER:
            myMatrix *= 1000.0f;
            break;
        case NIFTI_UNITS_MICRON:
            myMatrix *= 0.001f;
            break;
        default:
            return false;
    };
    sformOut = myMatrix.getMatrix();
    dimsOut[0] = myVol.m_volumeDimensions[0];
    dimsOut[1] = myVol.m_volumeDimensions[1];
    dimsOut[2] = myVol.m_volumeDimensions[2];
    return true;
}

void CiftiXML::setVolumeDimsAndSForm(const int64_t dims[3], const vector<vector<float> >& sform)
{
    CaretAssert(sform.size() == 3);
    if (m_root.m_matrices.size() == 0)
    {
        m_root.m_matrices.resize(1);
    }
    if (m_root.m_matrices[0].m_volume.size() == 0)
    {
        m_root.m_matrices[0].m_volume.resize(1);
    }
    CiftiVolumeElement& myVol = m_root.m_matrices[0].m_volume[0];
    if (myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ.size() == 0)
    {
        myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ.resize(1);
    }
    TransformationMatrixVoxelIndicesIJKtoXYZElement& myTrans = myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ[0];//oh the humanity
    for (int i = 0; i < 3; ++i)
    {
        CaretAssert(sform[i].size() == 4);
        for (int j = 0; j < 4; ++j)
        {
            myTrans.m_transform[i * 4 + j] = sform[i][j];
        }
    }
    myTrans.m_unitsXYZ = NIFTI_UNITS_MM;
    myVol.m_volumeDimensions[0] = dims[0];
    myVol.m_volumeDimensions[1] = dims[1];
    myVol.m_volumeDimensions[2] = dims[2];
}

AString CiftiXML::getMapName(const int& index, const int& myMapIndex) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return "#" + AString::number(index);
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_SCALARS &&
        myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_LABELS)
    {
        return "#" + AString::number(index);
    }
    CaretAssertVectorIndex(myMap.m_namedMaps, index);
    return myMap.m_namedMaps[index].m_mapName;
}

AString CiftiXML::getMapNameForColumnIndex(const int& index) const
{
    return getMapName(index, m_colMapIndex);
}

AString CiftiXML::getMapNameForRowIndex(const int& index) const
{
    return getMapName(index, m_rowMapIndex);
}

bool CiftiXML::setMapName(const int& index, const AString& name, const int& myMapIndex)
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_SCALARS &&
        myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_LABELS)
    {
        return false;
    }
    CaretAssertVectorIndex(myMap.m_namedMaps, index);
    myMap.m_namedMaps[index].m_mapName = name;
    return true;
}

bool CiftiXML::setMapNameForColumnIndex(const int& index, const AString& name)
{
    return setMapName(index, name, m_colMapIndex);
}

bool CiftiXML::setMapNameForRowIndex(const int& index, const AString& name)
{
    return setMapName(index, name, m_rowMapIndex);
}

const GiftiLabelTable* CiftiXML::getLabelTable(const int& index, const int& myMapIndex) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return NULL;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_LABELS)
    {
        return NULL;
    }
    CaretAssertVectorIndex(myMap.m_namedMaps, index);
    return myMap.m_namedMaps[index].m_labelTable;
}

const GiftiLabelTable* CiftiXML::getLabelTableForColumnIndex(const int& index) const
{
    return getLabelTable(index, m_colMapIndex);
}

const GiftiLabelTable* CiftiXML::getLabelTableForRowIndex(const int& index) const
{
    return getLabelTable(index, m_rowMapIndex);
}

bool CiftiXML::setLabelTable(const int& index, const GiftiLabelTable& labelTable, const int& myMapIndex)
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_LABELS)
    {
        return false;
    }
    CaretAssertVectorIndex(myMap.m_namedMaps, index);
    if (myMap.m_namedMaps[index].m_labelTable == NULL)//should never happen, but just in case
    {
        myMap.m_namedMaps[index].m_labelTable.grabNew(new GiftiLabelTable(labelTable));
    } else {
        *(myMap.m_namedMaps[index].m_labelTable) = labelTable;
    }
    return true;
}

bool CiftiXML::setLabelTableForColumnIndex(const int& index, const GiftiLabelTable& labelTable)
{
    return setLabelTable(index, labelTable, m_colMapIndex);
}

bool CiftiXML::setLabelTableForRowIndex(const int& index, const GiftiLabelTable& labelTable)
{
    return setLabelTable(index, labelTable, m_rowMapIndex);
}

bool CiftiXML::hasVolumeData(const int& myMapIndex) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (m_root.m_matrices[0].m_volume.size() == 0)
    {
        return false;
    }
    if (myMap == NULL || myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS)
    {
        return false;
    }
    const CiftiVolumeElement& myVol = m_root.m_matrices[0].m_volume[0];
    if (myVol.m_transformationMatrixVoxelIndicesIJKtoXYZ.size() == 0)
    {
        return false;
    }
    for (int64_t i = 0; i < (int64_t)myMap->m_brainModels.size(); ++i)
    {
        if (myMap->m_brainModels[i].m_modelType == CIFTI_MODEL_TYPE_VOXELS)
        {
            return true;
        }
    }
    return false;
}

bool CiftiXML::hasRowVolumeData() const
{
    return hasVolumeData(m_rowMapIndex);
}

bool CiftiXML::hasColumnVolumeData() const
{
    return hasVolumeData(m_colMapIndex);
}

bool CiftiXML::hasColumnSurfaceData(const caret::StructureEnum::Enum& structure) const
{
    return (findSurfaceModel(m_colMapIndex, structure) != NULL);
}

bool CiftiXML::hasRowSurfaceData(const caret::StructureEnum::Enum& structure) const
{
    return (findSurfaceModel(m_rowMapIndex, structure) != NULL);
}

bool CiftiXML::addSurfaceModelToColumns(const int& numberOfNodes, const StructureEnum::Enum& structure, const float* roi)
{
    separateMaps();
    return addSurfaceModel(m_colMapIndex, numberOfNodes, structure, roi);
}

bool CiftiXML::addSurfaceModelToRows(const int& numberOfNodes, const StructureEnum::Enum& structure, const float* roi)
{
    separateMaps();
    return addSurfaceModel(m_rowMapIndex, numberOfNodes, structure, roi);
}

bool CiftiXML::addSurfaceModel(const int& myMapIndex, const int& numberOfNodes, const StructureEnum::Enum& structure, const float* roi)
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS) return false;
    CiftiBrainModelElement tempModel;
    tempModel.m_brainStructure = structure;
    tempModel.m_modelType = CIFTI_MODEL_TYPE_SURFACE;
    tempModel.m_indexOffset = getNewRangeStart(myMapIndex);
    tempModel.m_surfaceNumberOfNodes = numberOfNodes;
    if (roi == NULL)
    {
        tempModel.m_indexCount = numberOfNodes;
    } else {
        tempModel.m_indexCount = 0;
        tempModel.m_nodeIndices.reserve(numberOfNodes);
        bool allNodes = true;
        for (int i = 0; i < numberOfNodes; ++i)
        {
            if (roi[i] > 0.0f)
            {
                tempModel.m_nodeIndices.push_back(i);
            } else {
                allNodes = false;
            }
        }
        if (allNodes)
        {
            tempModel.m_nodeIndices.clear();
        } else {
            tempModel.m_indexCount = (unsigned long long)tempModel.m_nodeIndices.size();
        }
    }
    myMap->m_brainModels.push_back(tempModel);
    myMap->m_brainModels.back().setupLookup();
    return true;
}

bool CiftiXML::addSurfaceModelToColumns(const int& numberOfNodes, const StructureEnum::Enum& structure, const vector<int64_t>& nodeList)
{
    separateMaps();
    return addSurfaceModel(m_colMapIndex, numberOfNodes, structure, nodeList);
}

bool CiftiXML::addSurfaceModelToRows(const int& numberOfNodes, const StructureEnum::Enum& structure, const vector<int64_t>& nodeList)
{
    separateMaps();
    return addSurfaceModel(m_rowMapIndex, numberOfNodes, structure, nodeList);
}

bool CiftiXML::addSurfaceModel(const int& myMapIndex, const int& numberOfNodes, const StructureEnum::Enum& structure, const vector<int64_t>& nodeList)
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    CaretAssertMessage(checkSurfaceNodes(nodeList, numberOfNodes), "node list has node numbers that don't exist in the surface");
    CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);//call the check function inside an assert so it never does the check in release builds
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS) return false;
    CiftiBrainModelElement tempModel;
    tempModel.m_brainStructure = structure;
    tempModel.m_modelType = CIFTI_MODEL_TYPE_SURFACE;
    tempModel.m_indexOffset = getNewRangeStart(myMapIndex);
    tempModel.m_surfaceNumberOfNodes = numberOfNodes;
    tempModel.m_indexCount = (int64_t)nodeList.size();
    if ((int)nodeList.size() == numberOfNodes)
    {
        bool sequential = true;
        for (int i = 0; i < numberOfNodes; ++i)
        {
            if (nodeList[i] != i)
            {
                sequential = false;
                break;
            }
        }
        if (!sequential)
        {
            tempModel.m_nodeIndices = nodeList;
        }
    } else {
        tempModel.m_nodeIndices = nodeList;
    }
    myMap->m_brainModels.push_back(tempModel);
    myMap->m_brainModels.back().setupLookup();
    return true;
}

bool CiftiXML::checkSurfaceNodes(const vector<int64_t>& nodeList, const int& numberOfNodes) const
{
    int listSize = (int)nodeList.size();
    for (int i = 0; i < listSize; ++i)
    {
        if (nodeList[i] < 0 || nodeList[i] >= numberOfNodes) return false;
    }
    return true;
}

bool CiftiXML::addVolumeModel(const int& myMapIndex, const vector<voxelIndexType>& ijkList, const StructureEnum::Enum& structure)
{
    separateMaps();
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS) return false;
    CaretAssertMessage(checkVolumeIndices(ijkList), "volume voxel list doesn't match cifti volume space, do setVolumeDimsAndSForm first");
    CiftiBrainModelElement tempModel;//call the check function inside an assert so it never does the check in release builds
    tempModel.m_brainStructure = structure;
    tempModel.m_modelType = CIFTI_MODEL_TYPE_VOXELS;
    tempModel.m_indexOffset = getNewRangeStart(myMapIndex);
    tempModel.m_indexCount = ijkList.size() / 3;
    tempModel.m_voxelIndicesIJK = ijkList;
    myMap->m_brainModels.push_back(tempModel);
    return true;
}

bool CiftiXML::addVolumeModelToColumns(const vector<voxelIndexType>& ijkList, const StructureEnum::Enum& structure)
{
    return addVolumeModel(m_colMapIndex, ijkList, structure);
}

bool CiftiXML::addVolumeModelToRows(const vector<voxelIndexType>& ijkList, const StructureEnum::Enum& structure)
{
    return addVolumeModel(m_rowMapIndex, ijkList, structure);
}

bool CiftiXML::addParcelSurfaceToColumns(const int& numberOfNodes, const StructureEnum::Enum& structure)
{
    separateMaps();
    if (numberOfNodes < 1 || m_colMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
    CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS) return false;
    CiftiParcelSurfaceElement tempSurf;
    tempSurf.m_numNodes = numberOfNodes;
    tempSurf.m_structure = structure;
    myMap.m_parcelSurfaces.push_back(tempSurf);
    myMap.setupLookup();//TODO: make the lookup maintenance incremental
    return true;
}

bool CiftiXML::addParcelSurfaceToRows(const int& numberOfNodes, const StructureEnum::Enum& structure)
{
    separateMaps();
    if (numberOfNodes < 1 || m_rowMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
    CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS) return false;
    CiftiParcelSurfaceElement tempSurf;
    tempSurf.m_numNodes = numberOfNodes;
    tempSurf.m_structure = structure;
    myMap.m_parcelSurfaces.push_back(tempSurf);
    myMap.setupLookup();//TODO: make the lookup maintenance incremental
    return true;
}

bool CiftiXML::addParcelToColumns(const CiftiParcelElement& parcel)
{
    separateMaps();
    if (m_colMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
    CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS) return false;
    if (!checkVolumeIndices(parcel.m_voxelIndicesIJK)) return false;
    myMap.m_parcels.push_back(parcel);//NOTE: setupLookup does error checking for nodes
    try
    {
        myMap.setupLookup();//TODO: make the lookup maintenance incremental, decide on throw vs bool return, separate sanity checking?
    } catch (...) {
        return false;
    }
    return true;
}

bool CiftiXML::addParcelToRows(const caret::CiftiParcelElement& parcel)
{
    separateMaps();
    if (m_rowMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return false;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
    CiftiMatrixIndicesMapElement& myMap = m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex];
    if (myMap.m_indicesMapToDataType != CIFTI_INDEX_TYPE_PARCELS) return false;
    if (!checkVolumeIndices(parcel.m_voxelIndicesIJK)) return false;
    myMap.m_parcels.push_back(parcel);//NOTE: setupLookup does error checking for nodes
    try
    {
        myMap.setupLookup();//TODO: make the lookup maintenance incremental, decide on throw vs bool return, separate sanity checking?
    } catch (...) {
        return false;
    }
    return true;
}

bool CiftiXML::checkVolumeIndices(const vector<voxelIndexType>& ijkList) const
{
    int64_t listSize = (int64_t)ijkList.size();
    if (listSize % 3 != 0) return false;
    int64_t dims[3];
    vector<vector<float> > sform;//not used, but needed by the funciton
    if (!getVolumeDimsAndSForm(dims, sform)) return false;
    for (int i = 0; i < listSize; i += 3)
    {
        if (ijkList[i] < 0 || ijkList[i] >= dims[0]) return false;
        if (ijkList[i + 1] < 0 || ijkList[i + 1] >= dims[1]) return false;
        if (ijkList[i + 2] < 0 || ijkList[i + 2] >= dims[2]) return false;
    }
    return true;
}

void CiftiXML::applyColumnMapToRows()
{
    if (m_rowMapIndex == m_colMapIndex) return;
    applyDimensionHelper(0, 1);
    m_rowMapIndex = m_colMapIndex;
}

void CiftiXML::applyRowMapToColumns()
{
    if (m_rowMapIndex == m_colMapIndex) return;
    applyDimensionHelper(1, 0);
    m_colMapIndex = m_rowMapIndex;
}

void CiftiXML::applyDimensionHelper(const int& from, const int& to)
{
    if (m_root.m_matrices.size() == 0) return;
    CiftiMatrixElement& myMatrix = m_root.m_matrices[0];//assume only one matrix
    int numMaps = (int)myMatrix.m_matrixIndicesMap.size();
    for (int i = 0; i < numMaps; ++i)
    {
        CiftiMatrixIndicesMapElement& myMap = myMatrix.m_matrixIndicesMap[i];
        int numDimensions = (int)myMap.m_appliesToMatrixDimension.size();
        for (int j = 0; j < numDimensions; ++j)
        {
            if (myMap.m_appliesToMatrixDimension[j] == to)
            {
                myMap.m_appliesToMatrixDimension.erase(myMap.m_appliesToMatrixDimension.begin() + j);
                --numDimensions;
                --j;
                break;
            }
        }
        for (int j = 0; j < numDimensions; ++j)
        {
            if (myMap.m_appliesToMatrixDimension[j] == from)
            {
                myMap.m_appliesToMatrixDimension.push_back(to);
                break;
            }
        }
        if (myMap.m_appliesToMatrixDimension.size() == 0)
        {
            myMatrix.m_matrixIndicesMap.erase(myMatrix.m_matrixIndicesMap.begin() + i);
            if (m_rowMapIndex > i) --m_rowMapIndex;
            if (m_colMapIndex > i) --m_colMapIndex;
            --numMaps;
            --i;//make sure we don't skip a map due to an erase
        }
    }
}

void CiftiXML::resetColumnsToBrainModels()
{
    if (m_colMapIndex == -1)
    {
        m_colMapIndex = createMap(0);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(0);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_BRAIN_MODELS;
    m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex] = myMap;
}

void CiftiXML::resetRowsToBrainModels()
{
    if (m_rowMapIndex == -1)
    {
        m_rowMapIndex = createMap(1);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(1);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_BRAIN_MODELS;
    m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex] = myMap;
}

void CiftiXML::resetColumnsToTimepoints(const float& timestep, const int& timepoints)
{
    if (m_colMapIndex == -1)
    {
        m_colMapIndex = createMap(0);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(0);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_TIME_POINTS;
    myMap.m_timeStepUnits = NIFTI_UNITS_SEC;
    myMap.m_timeStep = timestep;
    myMap.m_numTimeSteps = timepoints;
    m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex] = myMap;
}

void CiftiXML::resetRowsToTimepoints(const float& timestep, const int& timepoints)
{
    if (m_rowMapIndex == -1)
    {
        m_rowMapIndex = createMap(1);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(1);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_TIME_POINTS;
    myMap.m_timeStepUnits = NIFTI_UNITS_SEC;
    myMap.m_timeStep = timestep;
    myMap.m_numTimeSteps = timepoints;
    m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex] = myMap;
}

void CiftiXML::resetColumnsToScalars(const int& numMaps)
{
    if (m_colMapIndex == -1)
    {
        m_colMapIndex = createMap(0);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(0);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_SCALARS;
    myMap.m_namedMaps.resize(numMaps);
    m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex] = myMap;
}

void CiftiXML::resetRowsToScalars(const int& numMaps)
{
    if (m_rowMapIndex == -1)
    {
        m_rowMapIndex = createMap(1);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(1);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_SCALARS;
    myMap.m_namedMaps.resize(numMaps);
    m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex] = myMap;
}

void CiftiXML::resetColumnsToLabels(const int& numMaps)
{
    if (m_colMapIndex == -1)
    {
        m_colMapIndex = createMap(0);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(0);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_LABELS;
    myMap.m_namedMaps.resize(numMaps);
    for (int i = 0; i < numMaps; ++i)
    {
        myMap.m_namedMaps[i].m_labelTable.grabNew(new GiftiLabelTable());
    }
    m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex] = myMap;
}

void CiftiXML::resetRowsToLabels(const int& numMaps)
{
    if (m_rowMapIndex == -1)
    {
        m_rowMapIndex = createMap(1);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(1);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_LABELS;
    myMap.m_namedMaps.resize(numMaps);
    for (int i = 0; i < numMaps; ++i)
    {
        myMap.m_namedMaps[i].m_labelTable.grabNew(new GiftiLabelTable());
    }
    m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex] = myMap;
}

void CiftiXML::resetColumnsToParcels()
{
    if (m_colMapIndex == -1)
    {
        m_colMapIndex = createMap(0);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(0);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_PARCELS;
    m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex] = myMap;
}

void CiftiXML::resetRowsToParcels()
{
    if (m_rowMapIndex == -1)
    {
        m_rowMapIndex = createMap(1);
    } else {
        separateMaps();
    }
    CiftiMatrixIndicesMapElement myMap;
    myMap.m_appliesToMatrixDimension.push_back(1);
    myMap.m_indicesMapToDataType = CIFTI_INDEX_TYPE_PARCELS;
    m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex] = myMap;
}

int CiftiXML::createMap(int dimension)
{
    CiftiMatrixIndicesMapElement tempMap;
    tempMap.m_appliesToMatrixDimension.push_back(dimension);
    if (m_root.m_matrices.size() == 0)
    {
        m_root.m_matrices.resize(1);
        m_root.m_numberOfMatrices = 1;//TODO: remove this variable
    }
    CiftiMatrixElement& myMatrix = m_root.m_matrices[0];//assume only one matrix
    myMatrix.m_matrixIndicesMap.push_back(tempMap);
    return myMatrix.m_matrixIndicesMap.size() - 1;
}

void CiftiXML::separateMaps()
{
    if (m_root.m_matrices.size() == 0) return;
    CiftiMatrixElement& myMatrix = m_root.m_matrices[0];//assume only one matrix
    int numMaps = (int)myMatrix.m_matrixIndicesMap.size();
    for (int i = 0; i < numMaps; ++i)//don't need to loop over newly created maps
    {
        CiftiMatrixIndicesMapElement myMap = myMatrix.m_matrixIndicesMap[i];//make a copy because we are modifying this vector
        int numDimensions = (int)myMap.m_appliesToMatrixDimension.size();
        for (int j = 1; j < numDimensions; ++j)//leave the first in place
        {
            int whichDim = myMap.m_appliesToMatrixDimension[j];
            myMatrix.m_matrixIndicesMap.push_back(myMap);
            myMatrix.m_matrixIndicesMap.back().m_appliesToMatrixDimension.resize(1);
            myMatrix.m_matrixIndicesMap.back().m_appliesToMatrixDimension[0] = whichDim;
            if (whichDim == 1)
            {
                m_rowMapIndex = myMatrix.m_matrixIndicesMap.size() - 1;
            }
            if (whichDim == 0)
            {
                m_colMapIndex = myMatrix.m_matrixIndicesMap.size() - 1;
            }
        }
        myMatrix.m_matrixIndicesMap[i].m_appliesToMatrixDimension.resize(1);//ditch all but the first, they have their own maps
    }
}

int CiftiXML::getNewRangeStart(const int& myMapIndex) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        CaretAssert(false);
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    CaretAssert(myMap != NULL && myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_BRAIN_MODELS);
    int numModels = (int)myMap->m_brainModels.size();
    int curRet = 0;
    for (int i = 0; i < numModels; ++i)
    {
        int thisEnd = myMap->m_brainModels[i].m_indexOffset + myMap->m_brainModels[i].m_indexCount;
        if (thisEnd > curRet)
        {
            curRet = thisEnd;
        }
    }
    return curRet;
}

int CiftiXML::getNumberOfColumns() const
{//number of columns is LENGTH OF A ROW
    if (m_rowMapIndex == -1)
    {
        return 0;//unspecified should be an error, probably, but be permissive
    } else {
        if (m_root.m_matrices.size() == 0)
        {
            return 0;
        }
        CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
        const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex]);
        if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_TIME_POINTS)
        {
            return myMap->m_numTimeSteps;
        } else if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_BRAIN_MODELS) {
            return getNewRangeStart(m_rowMapIndex);
        } else if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_SCALARS || myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_LABELS) {
            return myMap->m_namedMaps.size();
        } else if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_PARCELS) {
            return myMap->m_parcels.size();
        } else {
            throw CiftiFileException("unknown cifti mapping type");
        }
    }
}

int CiftiXML::getNumberOfRows() const
{
    if (m_colMapIndex == -1)
    {
        return 0;//unspecified should be an error, probably, but be permissive
    } else {
        if (m_root.m_matrices.size() == 0)
        {
            return 0;
        }
        CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
        const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex]);
        if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_TIME_POINTS)
        {
            return myMap->m_numTimeSteps;
        } else if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_BRAIN_MODELS) {
            return getNewRangeStart(m_colMapIndex);
        } else if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_SCALARS || myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_LABELS) {
            return myMap->m_namedMaps.size();
        } else if (myMap->m_indicesMapToDataType == CIFTI_INDEX_TYPE_PARCELS) {
            return myMap->m_parcels.size();
        } else {
            throw CiftiFileException("unknown cifti mapping type");
        }
    }
}

IndicesMapToDataType CiftiXML::getColumnMappingType() const
{
    if (m_colMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return CIFTI_INDEX_TYPE_INVALID;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex]);
    return myMap->m_indicesMapToDataType;
}

IndicesMapToDataType CiftiXML::getRowMappingType() const
{
    if (m_rowMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return CIFTI_INDEX_TYPE_INVALID;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex]);
    return myMap->m_indicesMapToDataType;
}

const CiftiBrainModelElement* CiftiXML::findSurfaceModel(const int& myMapIndex, const StructureEnum::Enum& structure) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return NULL;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS) return NULL;
    const vector<CiftiBrainModelElement>& myModels = myMap->m_brainModels;
    int numModels = myModels.size();
    for (int i = 0; i < numModels; ++i)
    {
        if (myModels[i].m_modelType == CIFTI_MODEL_TYPE_SURFACE && myModels[i].m_brainStructure == structure)
        {
            return &(myModels[i]);
        }
    }
    return NULL;
}

const CiftiBrainModelElement* CiftiXML::findVolumeModel(const int& myMapIndex, const StructureEnum::Enum& structure) const
{
    if (myMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return NULL;
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, myMapIndex);
    const CiftiMatrixIndicesMapElement* myMap = &(m_root.m_matrices[0].m_matrixIndicesMap[myMapIndex]);
    if (myMap->m_indicesMapToDataType != CIFTI_INDEX_TYPE_BRAIN_MODELS) return NULL;
    const vector<CiftiBrainModelElement>& myModels = myMap->m_brainModels;
    int numModels = myModels.size();
    for (int i = 0; i < numModels; ++i)
    {
        if (myModels[i].m_modelType == CIFTI_MODEL_TYPE_VOXELS && myModels[i].m_brainStructure == structure)
        {
            return &(myModels[i]);
        }
    }
    return NULL;
}

bool CiftiXML::matchesForColumns(const CiftiXML& rhs) const
{
    if (this == &rhs) return true;//compare pointers to skip checking object against itself
    if (m_colMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return (rhs.m_colMapIndex == -1 || rhs.m_root.m_matrices.size() == 0);
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_colMapIndex);
    CaretAssertVectorIndex(rhs.m_root.m_matrices[0].m_matrixIndicesMap, rhs.m_colMapIndex);
    if (!matchesVolumeSpace(rhs)) return false;
    return (m_root.m_matrices[0].m_matrixIndicesMap[m_colMapIndex] == rhs.m_root.m_matrices[0].m_matrixIndicesMap[rhs.m_colMapIndex]);
}

bool CiftiXML::matchesForRows(const caret::CiftiXML& rhs) const
{
    if (this == &rhs) return true;//compare pointers to skip checking object against itself
    if (m_rowMapIndex == -1 || m_root.m_matrices.size() == 0)
    {
        return (rhs.m_rowMapIndex == -1 || rhs.m_root.m_matrices.size() == 0);
    }
    CaretAssertVectorIndex(m_root.m_matrices[0].m_matrixIndicesMap, m_rowMapIndex);
    CaretAssertVectorIndex(rhs.m_root.m_matrices[0].m_matrixIndicesMap, rhs.m_rowMapIndex);
    if (!matchesVolumeSpace(rhs)) return false;
    return (m_root.m_matrices[0].m_matrixIndicesMap[m_rowMapIndex] == rhs.m_root.m_matrices[0].m_matrixIndicesMap[rhs.m_rowMapIndex]);
}

bool CiftiXML::operator==(const caret::CiftiXML& rhs) const
{
    if (this == &rhs) return true;//compare pointers to skip checking object against itself
    if (m_root.m_matrices.size() != rhs.m_root.m_matrices.size()) return false;
    if (!matchesVolumeSpace(rhs)) return false;
    if (!matchesForColumns(rhs)) return false;
    if (m_root.m_matrices.size() > 1) return matchesForRows(rhs);
    return true;
}

bool CiftiXML::matchesVolumeSpace(const CiftiXML& rhs) const
{
    if (hasColumnVolumeData() || hasRowVolumeData())
    {
        if (!(rhs.hasColumnVolumeData() || rhs.hasRowVolumeData()))
        {
            return false;
        }
    } else {
        if (rhs.hasColumnVolumeData() || rhs.hasRowVolumeData())
        {
            return false;
        } else {
            return true;//don't check for matching/existing sforms if there are no voxel maps in either
        }
    }
    int64_t dims[3], rdims[3];
    vector<vector<float> > sform, rsform;
    if (!getVolumeDimsAndSForm(dims, sform) || !rhs.getVolumeDimsAndSForm(rdims, rsform))
    {//should NEVER happen
        CaretAssertMessage(false, "has*VolumeData() and getVolumeDimsAndSForm() disagree");
        throw CiftiFileException("has*VolumeData() and getVolumeDimsAndSForm() disagree");
    }
    const float TOLER_RATIO = 0.999f;//ratio a spacing element can mismatch by
    for (int i = 0; i < 3; ++i)
    {
        if (dims[i] != rdims[i]) return false;
        for (int j = 0; j < 4; ++j)
        {
            float left = sform[i][j];
            float right = rsform[i][j];
            if (left != right && (left == 0.0f || right == 0.0f || left / right < TOLER_RATIO || right / left < TOLER_RATIO)) return false;
        }
    }
    return true;
}
