/*LICENSE_START*/
/*
 *  Copyright 1995-2002 Washington University School of Medicine
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

#include "OperationMetricMask.h"
#include "OperationException.h"
#include "MetricFile.h"
#include "PaletteColorMapping.h"

using namespace caret;
using namespace std;

AString OperationMetricMask::getCommandSwitch()
{
    return "-metric-mask";
}

AString OperationMetricMask::getShortDescription()
{
    return "MASK A METRIC FILE";
}

OperationParameters* OperationMetricMask::getParameters()
{
    OperationParameters* ret = new OperationParameters();
    ret->addMetricParameter(1, "metric", "the input metric");
    ret->addMetricParameter(2, "mask", "the mask metric");
    ret->addMetricOutputParameter(3, "metric-out", "the output metric");
    OptionalParameter* columnSelect = ret->createOptionalParameter(4, "-column", "select a single column");
    columnSelect->addIntegerParameter(1, "column-number", "the column number");
    ret->setHelpText(
        AString("By default, the output metric is a copy of the input metric, but with zeros wherever the mask metric is not positive.  ") +
        "if -column is specified, the output contains only one column, the masked version of the specified input column."
    );
    return ret;
}

void OperationMetricMask::useParameters(OperationParameters* myParams, ProgressObject* myProgObj)
{
    LevelProgress myProgress(myProgObj);
    MetricFile* myMetric = myParams->getMetric(1);
    MetricFile* myMask = myParams->getMetric(2);
    int32_t numNodes = myMetric->getNumberOfNodes();
    if (myMask->getNumberOfNodes() != numNodes)
    {
        throw OperationException("mask metric must have the same number of nodes");
    }
    MetricFile* myMetricOut = myParams->getOutputMetric(3);//gets the output metric with key 2
    OptionalParameter* columnSelect = myParams->getOptionalParameter(4);//gets optional parameter with key 3
    int64_t columnNum = -1;
    if (columnSelect->m_present)
    {//set up to use the single column
        columnNum = columnSelect->getInteger(1);
        if (columnNum < 0 || columnNum >= myMetric->getNumberOfColumns())
        {
            throw OperationException("invalid column specified");
        }
    }
    CaretArray<float> myColumnOut(numNodes);
    if (columnNum == -1)
    {
        int32_t numCols = myMetric->getNumberOfColumns();
        myMetricOut->setNumberOfNodesAndColumns(numNodes, numCols);
        const float* maskVals = myMask->getValuePointerForColumn(0);
        for (int32_t col = 0; col < numCols; ++col)
        {
            const float* metricVals = myMetric->getValuePointerForColumn(col);
            *(myMetricOut->getPaletteColorMapping(col)) = *(myMetric->getPaletteColorMapping(col));//copy the palette settings
            for (int32_t node = 0; node < numNodes; ++node)
            {
                if (maskVals[node] > 0.0f)
                {
                    myColumnOut[node] = metricVals[node];
                } else {
                    myColumnOut[node] = 0.0f;
                }
            }
            myMetricOut->setValuesForColumn(col, myColumnOut.getArray());
            myMetricOut->setColumnName(col, myMetric->getColumnName(col));
        }
    } else {
        myMetricOut->setNumberOfNodesAndColumns(numNodes, 1);
        const float* maskVals = myMask->getValuePointerForColumn(0);
        const float* metricVals = myMetric->getValuePointerForColumn(columnNum);
        *(myMetricOut->getPaletteColorMapping(0)) = *(myMetric->getPaletteColorMapping(columnNum));//copy the palette settings
        for (int32_t node = 0; node < numNodes; ++node)
        {
            if (maskVals[node] > 0.0f)
            {
                myColumnOut[node] = metricVals[node];
            } else {
                myColumnOut[node] = 0.0f;
            }
        }
        myMetricOut->setValuesForColumn(0, myColumnOut.getArray());
        myMetricOut->setColumnName(0, myMetric->getColumnName(columnNum));
    }
}
