#ifndef __EVENT_NODE_DATA_FILES_GET_H__
#define __EVENT_NODE_DATA_FILES_GET_H__

/*LICENSE_START*/
/*
 *  Copyright (C) 2014  Washington University School of Medicine
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*LICENSE_END*/

#include "Event.h"

namespace caret {

    class GiftiTypeFile;
    class LabelFile;
    class MetricFile;
    class Model;
    class RgbaFile;
    class Surface;
    
    /// Event that gets all node data files.
    class EventNodeDataFilesGet : public Event {
        
    public:
        EventNodeDataFilesGet();
        
        EventNodeDataFilesGet(const Surface* surface);
        
        virtual ~EventNodeDataFilesGet();
        
        void addFile(GiftiTypeFile* nodeDataFile);        
        
        /**
         * @return Returns the surface for which associated data
         * files are requested.  If NULL, then node data files
         * from all brain structures are requested. 
         */
        const Surface* getSurface() const { return this->surface; }
        
        /** @return Returns the label files. */
        std::vector<LabelFile*> getLabelFiles() const { return this->labelFiles; }
        
        /** @return Returns the metric files. */
        std::vector<MetricFile*> getMetricFiles() const { return this->metricFiles; }
        
        /** @return Returns the rgba files. */
        std::vector<RgbaFile*> getRgbaFiles() const { return this->rgbaFiles; }
        
        void getAllFiles(std::vector<GiftiTypeFile*>& allFilesOut) const;
        
    private:
        EventNodeDataFilesGet(const EventNodeDataFilesGet&);
        
        EventNodeDataFilesGet& operator=(const EventNodeDataFilesGet&);
        
        const Surface* surface;
        
        std::vector<LabelFile*> labelFiles;
        
        std::vector<MetricFile*> metricFiles;
        
        std::vector<RgbaFile*> rgbaFiles;
        
    };

} // namespace

#endif // __EVENT_NODE_DATA_FILES_GET_H__
