#ifndef CSV_COMPONENT_DATA_HPP
#define CSV_COMPONENT_DATA_HPP

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include "../../api/framework/component_types.hpp"

/**
 * @brief Data structure for CSV component
 * 
 * Following EPOS SmartData principles, this structure contains all the data
 * needed for CSV file reading and data serialization functionality.
 * Reads CSV files in the format: timestamp,id,lat,lon,alt,x,y,z,speed,heading,yawrate,acceleration
 * and provides serialization into std::vector<std::uint8_t> for message transmission.
 * 
 * Replaces inheritance-based data storage with pure composition.
 */
struct CSVComponentData : public ComponentData {
    // CSV data structure - matches dynamics-vehicle_0.csv format
    struct CSVRecord {
        std::uint64_t timestamp;
        std::uint32_t id;
        double lat;
        double lon;
        double alt;
        double x;
        double y;
        double z;
        double speed;
        double heading;
        double yawrate;
        double acceleration;
    };
    
    // CSV file management
    std::string csv_file_path;
    std::vector<CSVRecord> csv_data;
    std::size_t current_row_index;
    bool file_loaded;
    
    // Statistics
    std::size_t total_records;
    std::size_t records_sent;
    
    /**
     * @brief Constructor initializes CSV component state
     * 
     * Sets up the CSV component with default values. The CSV file path
     * should be set using load_csv_file() method after construction.
     */
    CSVComponentData() : current_row_index(0), 
                        file_loaded(false), 
                        total_records(0), 
                        records_sent(0) {}
    
    /**
     * @brief Load CSV file and parse its contents
     * 
     * Loads and parses a CSV file with the expected format:
     * timestamp,id,lat,lon,alt,x,y,z,speed,heading,yawrate,acceleration
     * 
     * @param file_path Path to the CSV file to load
     * @return true if file was loaded successfully, false otherwise
     */
    bool load_csv_file(const std::string& file_path) {
        csv_file_path = file_path;
        csv_data.clear();
        current_row_index = 0;
        file_loaded = false;
        total_records = 0;
        records_sent = 0;
        
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return false;
        }
        
        std::string line;
        bool first_line = true;
        
        // Read CSV file line by line
        while (std::getline(file, line)) {
            // Skip header line
            if (first_line) {
                first_line = false;
                continue;
            }
            
            // Parse CSV line
            CSVRecord record;
            if (parse_csv_line(line, record)) {
                csv_data.push_back(record);
            }
        }
        
        file.close();
        total_records = csv_data.size();
        file_loaded = (total_records > 0);
        
        return file_loaded;
    }
    
    /**
     * @brief Parse a single CSV line into a CSVRecord
     * 
     * @param line CSV line to parse
     * @param record Reference to CSVRecord to populate
     * @return true if parsing was successful, false otherwise
     */
    bool parse_csv_line(const std::string& line, CSVRecord& record) {
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        // Split line by commas
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        // Ensure we have the expected number of columns (12)
        if (tokens.size() != 12) {
            return false;
        }
        
        try {
            record.timestamp = std::stoull(tokens[0]);
            record.id = std::stoul(tokens[1]);
            record.lat = std::stod(tokens[2]);
            record.lon = std::stod(tokens[3]);
            record.alt = std::stod(tokens[4]);
            record.x = std::stod(tokens[5]);
            record.y = std::stod(tokens[6]);
            record.z = std::stod(tokens[7]);
            record.speed = std::stod(tokens[8]);
            record.heading = std::stod(tokens[9]);
            record.yawrate = std::stod(tokens[10]);
            record.acceleration = std::stod(tokens[11]);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    /**
     * @brief Get the next CSV record
     * 
     * Returns the next CSV record in sequence. If the end of file is reached,
     * it wraps around to the beginning (circular reading).
     * 
     * @return Pointer to the next CSVRecord, or nullptr if no data is loaded
     */
    const CSVRecord* get_next_record() {
        if (!file_loaded || csv_data.empty()) {
            return nullptr;
        }
        
        const CSVRecord* record = &csv_data[current_row_index];
        current_row_index = (current_row_index + 1) % csv_data.size();
        records_sent++;
        
        return record;
    }
    
    /**
     * @brief Reset the CSV reader to the beginning
     */
    void reset() {
        current_row_index = 0;
        records_sent = 0;
    }
    
    /**
     * @brief Get CSV loading status
     * 
     * @return true if CSV file is loaded and ready, false otherwise
     */
    bool is_loaded() const {
        return file_loaded;
    }
    
    /**
     * @brief Get total number of records in the CSV
     * 
     * @return Total number of CSV records loaded
     */
    std::size_t get_total_records() const {
        return total_records;
    }
    
    /**
     * @brief Get number of records sent so far
     * 
     * @return Number of records that have been sent
     */
    std::size_t get_records_sent() const {
        return records_sent;
    }
    
    /**
     * @brief Get the size of a serialized CSV record in bytes
     * 
     * @return Size of a CSVRecord when serialized to bytes
     */
    static std::size_t get_record_size() {
        return sizeof(CSVRecord);
    }
};

#endif // CSV_COMPONENT_DATA_HPP 