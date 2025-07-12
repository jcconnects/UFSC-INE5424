#ifndef CSV_COMPONENT_FUNCTIONS_HPP
#define CSV_COMPONENT_FUNCTIONS_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include "csv_component_data.hpp"
#include "../../api/framework/component_functions.hpp"
#include "../../api/util/debug.h"

/**
 * @brief Producer function for CSV component - reads and serializes CSV data
 * 
 * Replaces inheritance-based CSV component with direct function call.
 * Reads the next record from the loaded CSV file and serializes it into
 * a byte vector for message transmission. The CSV data structure matches
 * the format: timestamp,id,lat,lon,alt,x,y,z,speed,heading,yawrate,acceleration
 * 
 * If no CSV file is loaded or no data is available, returns an empty vector.
 * The component automatically cycles through the CSV data when it reaches the end.
 * 
 * @param unit The data unit being requested (should be DataTypes::CSV_VEHICLE_DATA)
 * @param data Pointer to CSVComponentData structure containing CSV data
 * @return Vector containing serialized CSV record data
 */
inline std::vector<std::uint8_t> csv_producer(std::uint32_t unit, ComponentData* data) {
    CSVComponentData* csv_data = static_cast<CSVComponentData*>(data);
    
    if (!csv_data) {
        db<void>(WRN) << "[CSVComponent] Received null data pointer\n";
        return std::vector<std::uint8_t>();
    }
    
    // Check if CSV file is loaded
    if (!csv_data->is_loaded()) {
        db<void>(WRN) << "[CSVComponent] No CSV file loaded or no data available\n";
        return std::vector<std::uint8_t>();
    }
    
    // Get the next CSV record
    const CSVComponentData::CSVRecord* record = csv_data->get_next_record();
    if (!record) {
        db<void>(ERR) << "[CSVComponent] Failed to get next CSV record\n";
        return std::vector<std::uint8_t>();
    }
    
    // Serialize CSV record to byte vector with timestamp prepended
    std::vector<std::uint8_t> result(sizeof(std::uint64_t) + sizeof(CSVComponentData::CSVRecord));
    
    // Copy timestamp to the beginning of the result vector
    std::memcpy(result.data(), &record->timestamp, sizeof(std::uint64_t));
    
    // Copy the entire CSV record after the timestamp
    std::memcpy(result.data() + sizeof(std::uint64_t), record, sizeof(CSVComponentData::CSVRecord));
    
    // Log CSV data transmission
    db<void>(INF) << "[CSVComponent] Sending CSV record #" << csv_data->get_records_sent()
                  << " (" << csv_data->get_records_sent() << "/" << csv_data->get_total_records() << ")"
                  << " timestamp: " << record->timestamp
                  << " id: " << record->id
                  << " pos: (" << record->x << ", " << record->y << ", " << record->z << ")"
                  << " speed: " << record->speed
                  << " size: " << result.size() << " bytes\n";
    
    // Additional detailed logging for debugging (key field values)
    db<void>(TRC) << "[CSVComponent] Record details: "
                  << "lat=" << record->lat << ", "
                  << "lon=" << record->lon << ", "
                  << "alt=" << record->alt << ", "
                  << "heading=" << record->heading << ", "
                  << "yawrate=" << record->yawrate << ", "
                  << "acceleration=" << record->acceleration << "\n";
    
    return result;
}

/**
 * @brief Consumer function for CSV component (not implemented - CSV is producer-only)
 * 
 * CSV components are typically producer-only and don't consume data from other
 * components. They read data from CSV files and transmit it to other components.
 * This function is provided for interface compatibility but performs no operations
 * as requested by the user specification.
 * 
 * @param msg Pointer to the received message (unused)
 * @param data Pointer to CSVComponentData structure (unused)
 */
inline void csv_consumer(void* msg, ComponentData* data) {
    // CSV component has no consumer implementation as specified by user
    // This function exists for interface compatibility but performs no operations
    db<void>(TRC) << "[CSVComponent] Consumer function called (CSV component has no consumer implementation)\n";
}

#endif // CSV_COMPONENT_FUNCTIONS_HPP 